# Fliiper Keys — Claude Code project guide

A Flipper Zero external app (`.fap`) that acts as a USB-HID password typer.
Secrets live in an **encrypted** vault on the SD card; selecting a domain makes
the Flipper type the matching password on the connected PC as if from a keyboard.

This file is read automatically by Claude Code. Keep it accurate.

## What this app does (and doesn't)

- At launch, asks which **host keyboard layout** to type with (built-in US, or
  any `.kl` file found under `/ext/badusb/assets/layouts`).
- Reads `/ext/apps_data/fliiper_keys/vault.fkm` (AES-256-CTR encrypted).
- Prompts for a passphrase, derives the key, decrypts in RAM.
- Lists **domains only**. Passwords are never shown on screen.
- On select: switches USB to HID keyboard, types the password, restores USB.
- It does **not** "inject" into a field — HID has no concept of fields. It
  types scancodes; the focused field receives them. Put the cursor in the
  password box first.

Intended use: typing **your own** passwords onto **your own** machines. That is
the normal job of a password manager.

## Build & flash (uFBT)

```bash
pip install --upgrade ufbt
./tools/fetch_deps.sh        # one-time: vendored AES + SHA-256 into src/vendor/
ufbt                         # build -> dist/fliiper_keys.fap
ufbt launch                  # build, upload over USB, and run on the Flipper
ufbt cli                     # serial console for FURI_LOG output
```

`ufbt` pins its own copy of the firmware SDK, so you don't clone the whole
firmware tree. If headers are missing, run `ufbt update` to match your
installed firmware channel (release / rc / dev).

## Creating the vault

```bash
pip install cryptography
printf 'github.com:myp4ss\nmail.ch:other-pass\n' > accounts.txt
./tools/fkm_vault.py encrypt accounts.txt vault.fkm
shred -u accounts.txt        # destroy the plaintext
# copy vault.fkm to the SD card under apps_data/fliiper_keys/
./tools/fkm_vault.py decrypt vault.fkm   # verify it round-trips
```

## Layout / code map

```
application.fam          app manifest (entry point, icon, cdefines for tiny-AES)
src/
  keys_manager.{c,h}     app lifecycle, views, navigation, vault loading
  vault.{c,h}            parse "domain:password" plaintext -> entries
  crypto.{c,h}           KDF + AES-256-CTR file decryption; file format
  hid_layout.{c,h}       ASCII->HID layout table: built-in US + .kl loader
  hid_typer.{c,h}        USB HID typing, driven by a layout table
  vendor/                fetched: aes.{c,h} (tiny-AES), sha256.{c,h} (B-Con)
tools/
  fkm_vault.py           host-side encrypt/decrypt (MUST match crypto.c)
  fetch_deps.sh          downloads the vendored crypto primitives
icons/                   10x10 1-bit app icon
```

## Invariants Claude must preserve

- **crypto.c and tools/fkm_vault.py are a matched pair.** The file format,
  the KDF (iterated SHA-256), `FKM_KDF_ITERATIONS`, and AES-256-CTR semantics
  must stay byte-identical in both. Change one → change the other → re-run the
  round-trip check.
- **Never render a password**, never write a decrypted password to logs
  (`FURI_LOG_*`) or to the SD card. Only `domain` is display/log-safe.
- **Wipe secrets** (`memset` to 0) before freeing: passphrase buffer,
  derived keys, decrypted plaintext, entry passwords. See existing `memset`s.
- The first ':' splits domain from password, so passwords may contain ':'.

## Known limitations / roadmap (good first tasks)

1. **Keyboard layout.** (Partly done.) The app loads Flipper BadUSB `.kl`
   layout files and lets you pick one at launch, with built-in US fallback.
   Remaining: remember the last choice (config file) to skip the chooser; add
   a few layouts to the repo so it works without BadUSB assets installed.
2. **No integrity (MAC).** Add HMAC-SHA256 over `iv || ciphertext` so a wrong
   passphrase (or tampering) fails loudly instead of yielding garbage. Update
   both crypto.c and fkm_vault.py.
3. **Weak KDF.** Iterated SHA-256 is a stopgap. Move to PBKDF2-HMAC-SHA256
   (or scrypt) with a stored iteration count in the header.
4. **On-device editing.** Currently read-only; the vault is built on a PC.
   Adding/editing entries on the Flipper is a larger feature.
5. **Clipboard/Enter options, search filter** in the list view.

## Conventions

- C formatting matches Flipper: run `ufbt format` (clang-format) before commit.
- Tag for logs: `FKM_TAG` ("FliiperKeys").
- Keep `src/vendor/` untouched (upstream code); wrap it, don't edit it.

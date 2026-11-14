# Fliiper Keys

A Flipper Zero app that types your passwords over USB, from an **encrypted**
vault on the SD card. You pick a domain on the Flipper; it acts as a USB
keyboard and types the matching password into whatever field has focus on your
PC. A more ergonomic take on the stock BadUSB flow, dedicated to passwords.

> **Scope.** This is for typing *your own* passwords onto *your own* machines —
> the normal job of a password manager. Treat the vault like the secret it is.

## How it works

The Flipper can impersonate a USB keyboard. It doesn't "inject" into a password
box — HID has no notion of fields — it **types scancodes**, exactly as if you
were typing. So: click the password field first, then select the entry on the
Flipper.

The vault is `vault.fkm`, AES-256-CTR encrypted, built on your PC. On launch the
app asks for a passphrase, decrypts in RAM, and shows the list of **domains**
(passwords are never displayed). The Flipper's own PIN protects the device;
the passphrase protects the file.

## Quick start

```bash
# 1. Toolchain + crypto deps
pip install --upgrade ufbt cryptography
./tools/fetch_deps.sh

# 2. Build & flash (Flipper connected over USB)
ufbt launch

# 3. Make a vault
printf 'github.com:myp4ss\nmail.ch:other-pass\n' > accounts.txt
./tools/fkm_vault.py encrypt accounts.txt vault.fkm
shred -u accounts.txt

# 4. Copy vault.fkm to the SD card at:
#      /ext/apps_data/fliiper_keys/vault.fkm
```

Run **Fliiper Keys** from `Apps > Tools`, enter the passphrase, pick an account.

## Vault format

Plaintext, one entry per line, split on the **first** colon (so passwords may
contain `:`):

```
domain:password
# lines starting with # are ignored
mail.ch:pa:ss:word   # -> domain "mail.ch", password "pa:ss:word"
```

## Honest limitations

- **Keyboard layout:** US QWERTY only for now. On a Swiss/French host, symbols
  will mistype. See roadmap item #1 in `CLAUDE.md`.
- **No integrity check (MAC):** a wrong passphrase yields garbage rather than a
  clean "wrong password" error. Roadmap #2.
- **KDF is a stopgap** (iterated SHA-256), not PBKDF2/scrypt. Roadmap #3.
- Read-only: build the vault on a PC, not on the device.

## Repo layout & next steps

See `CLAUDE.md` for the full code map, invariants, and the roadmap. This repo is
set up for Claude Code: project guidance (`CLAUDE.md`), two subagents
(`flipper-dev`, `crypto-reviewer`), a `flipper-app-dev` skill, and `.mcp.json`.

## Credits / licenses

- `src/vendor/aes.*` — kokke/tiny-AES-c (public domain / Unlicense), fetched.
- `src/vendor/sha256.*` — B-Con/crypto-algorithms (public domain), fetched.

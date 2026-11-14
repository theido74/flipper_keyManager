---
name: flipper-app-dev
description: How to build, flash, debug, and structure a Flipper Zero external app (.fap) with uFBT. Use when working on this project's firmware — build errors, adding a GUI view, the application.fam manifest, reading SD files, USB HID, or on-device logs.
---

# Flipper Zero app development (uFBT)

## Toolchain
- `pip install --upgrade ufbt` — self-contained SDK, no full firmware clone.
- `ufbt` builds `dist/<appid>.fap`. `ufbt launch` builds + uploads + runs.
- `ufbt cli` opens the serial console; `FURI_LOG_*` output appears there.
- `ufbt update` re-pins the SDK to your firmware channel (release/rc/dev).
  Most "unknown symbol / header not found" errors are a channel mismatch.
- `ufbt format` applies the project clang-format.

## application.fam essentials
- `appid`, `entry_point` (a function `int32_t <name>(void*)`), `apptype=EXTERNAL`.
- `requires=["gui","dialogs","storage", ...]` pulls in needed records.
- `fap_icon` is a 10x10 1-bit PNG; `fap_icon_assets` points at the icon dir.
- `cdefines=[...]` passes -D flags (this project sets tiny-AES to AES256+CTR).

## GUI model
- `ViewDispatcher` owns multiple `View`s, each identified by an enum id.
- Common modules: `Submenu` (lists), `TextInput` (string entry), `Widget`
  (free-form text/graphics), `Popup`, `VariableItemList` (settings).
- `view_dispatcher_switch_to_view(vd, id)` changes screens.
- Register a navigation callback for the Back button; return true if handled,
  or call `view_dispatcher_stop(vd)` to exit the app.

## Storage
- Open `RECORD_STORAGE`; use `storage_file_alloc/open/read/write/close/free`.
- App-private data path macro: `APP_DATA_PATH("file")` →
  `/ext/apps_data/<appid>/file`. Create the folder on the SD card first.

## USB HID (keyboard emulation)
- `furi_hal_usb_set_config(&usb_hid, NULL)` switches the device to HID; save the
  previous config (`furi_hal_usb_get_config`) and restore it after.
- `furi_hal_hid_is_connected()` tells you the host enumerated the device.
- `furi_hal_hid_kb_press(code)` / `_release(code)` where `code` is a HID usage
  (e.g. `HID_KEYBOARD_A`) optionally OR'd with a modifier (`KEY_MOD_LEFT_SHIFT`).
- HID sends scancodes; the resulting character depends on the **host** keyboard
  layout. Reuse BadUSB `.kl` layouts for non-US hosts.

## Debugging checklist
1. `ufbt cli` and watch `FURI_LOG_*` with your `FKM_TAG`.
2. Reproduce, find root cause, make the minimal fix.
3. On crash (red screen), note the error; check for NULL derefs, freed-then-used
   views, and stack size (`stack_size` in the manifest).
4. Verify on real hardware — the emulator does not cover USB HID.

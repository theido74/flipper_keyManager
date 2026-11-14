#pragma once

// Keyboard layout = a 128-entry ASCII -> HID code table. Each entry is a
// uint16: low byte = HID usage code, high byte = modifier bits
// (KEY_MOD_LEFT_SHIFT, KEY_MOD_RIGHT_ALT for AltGr, ...). 0 = unsupported.
//
// This is exactly the format Flipper's BadUSB layout files (.kl) use, so we
// can load those directly instead of hand-writing QWERTZ/AZERTY. A .kl file
// is 256 little-endian uint16 entries; we use the first 128 (ASCII).
//
// Built-in US QWERTY is always available as a fallback.

#include <storage/storage.h>
#include <stdint.h>
#include <stdbool.h>

#define HID_LAYOUT_SIZE 128
#define HID_LAYOUTS_DIR "/ext/badusb/assets/layouts"

// Fill `table` with the built-in US QWERTY layout.
void hid_layout_load_us(uint16_t table[HID_LAYOUT_SIZE]);

// Load a .kl file into `table`. Returns false (and leaves `table` untouched)
// if the file is missing or too short.
bool hid_layout_load_file(Storage* storage, const char* path, uint16_t table[HID_LAYOUT_SIZE]);

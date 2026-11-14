#pragma once

#include "hid_layout.h"
#include <stdbool.h>

// Switches the USB stack to HID keyboard, "types" `text` on the host using
// `layout` (an ASCII->HID table from hid_layout.{c,h}), then restores the
// previous USB config.
//
// HID sends scancodes, so what actually appears on screen depends on `layout`
// matching the HOST keyboard layout. Pick the right one at startup.
//
// Returns false if the USB HID interface never became ready (e.g. not plugged
// into a host, or the host ignored the device).
bool hid_type_string(
    const uint16_t layout[HID_LAYOUT_SIZE],
    const char* text,
    bool press_enter_after);

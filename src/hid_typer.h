#pragma once

#include <stdbool.h>

// Switches the USB stack to HID keyboard, "types" `text` keystroke by
// keystroke on the host, then restores the previous USB config.
//
// IMPORTANT — keyboard layout:
//   HID sends *scancodes*, not characters. The character that actually
//   appears depends on the layout selected ON THE HOST (the PC), not on
//   the Flipper. The table in hid_typer.c is US QWERTY. On a Swiss
//   (QWERTZ) or French (AZERTY) host, symbols and some letters will come
//   out wrong until you add the matching layout. See the TODO there.
//
// Returns false if the USB HID interface never became ready (e.g. not
// plugged into a host, or the host ignored the device).
bool hid_type_string(const char* text, bool press_enter_after);

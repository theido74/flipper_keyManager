#include "hid_typer.h"

#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

#define FKM_TAG "FliiperKeys"

// Wait up to this long for the host to enumerate the HID interface.
#define HID_READY_TIMEOUT_MS 2000
// Delay between key events; too fast and some hosts drop characters.
#define HID_KEY_DELAY_MS 8

// ---------------------------------------------------------------------------
// ASCII -> HID usage code, US QWERTY. High bits carry modifiers
// (KEY_MOD_LEFT_SHIFT) exactly like furi_hal_hid_kb_press() expects.
//
// TODO(layout): add CH (QWERTZ) and FR (AZERTY) tables and a setting to
// pick one. The cleanest route is to reuse Flipper's BadUSB layout files
// (.kl) under /ext/badusb/assets/layouts and load the chosen one.
// Until then, set your HOST keyboard layout to US while testing.
// ---------------------------------------------------------------------------
static uint16_t ascii_to_hid(char c) {
    // Letters.
    if(c >= 'a' && c <= 'z') return HID_KEYBOARD_A + (c - 'a');
    if(c >= 'A' && c <= 'Z') return (HID_KEYBOARD_A + (c - 'A')) | KEY_MOD_LEFT_SHIFT;

    // Digits (1..9,0).
    switch(c) {
    case '1': return HID_KEYBOARD_1;
    case '2': return HID_KEYBOARD_2;
    case '3': return HID_KEYBOARD_3;
    case '4': return HID_KEYBOARD_4;
    case '5': return HID_KEYBOARD_5;
    case '6': return HID_KEYBOARD_6;
    case '7': return HID_KEYBOARD_7;
    case '8': return HID_KEYBOARD_8;
    case '9': return HID_KEYBOARD_9;
    case '0': return HID_KEYBOARD_0;

    // Unshifted punctuation.
    case ' ': return HID_KEYBOARD_SPACEBAR;
    case '-': return HID_KEYBOARD_MINUS;
    case '=': return HID_KEYBOARD_EQUAL_SIGN;
    case '[': return HID_KEYBOARD_OPEN_BRACKET;
    case ']': return HID_KEYBOARD_CLOSE_BRACKET;
    case '\\': return HID_KEYBOARD_BACKSLASH;
    case ';': return HID_KEYBOARD_SEMICOLON;
    case '\'': return HID_KEYBOARD_APOSTROPHE;
    case '`': return HID_KEYBOARD_GRAVE_ACCENT;
    case ',': return HID_KEYBOARD_COMMA;
    case '.': return HID_KEYBOARD_DOT;
    case '/': return HID_KEYBOARD_SLASH;

    // Shifted punctuation.
    case '!': return HID_KEYBOARD_1 | KEY_MOD_LEFT_SHIFT;
    case '@': return HID_KEYBOARD_2 | KEY_MOD_LEFT_SHIFT;
    case '#': return HID_KEYBOARD_3 | KEY_MOD_LEFT_SHIFT;
    case '$': return HID_KEYBOARD_4 | KEY_MOD_LEFT_SHIFT;
    case '%': return HID_KEYBOARD_5 | KEY_MOD_LEFT_SHIFT;
    case '^': return HID_KEYBOARD_6 | KEY_MOD_LEFT_SHIFT;
    case '&': return HID_KEYBOARD_7 | KEY_MOD_LEFT_SHIFT;
    case '*': return HID_KEYBOARD_8 | KEY_MOD_LEFT_SHIFT;
    case '(': return HID_KEYBOARD_9 | KEY_MOD_LEFT_SHIFT;
    case ')': return HID_KEYBOARD_0 | KEY_MOD_LEFT_SHIFT;
    case '_': return HID_KEYBOARD_MINUS | KEY_MOD_LEFT_SHIFT;
    case '+': return HID_KEYBOARD_EQUAL_SIGN | KEY_MOD_LEFT_SHIFT;
    case '{': return HID_KEYBOARD_OPEN_BRACKET | KEY_MOD_LEFT_SHIFT;
    case '}': return HID_KEYBOARD_CLOSE_BRACKET | KEY_MOD_LEFT_SHIFT;
    case '|': return HID_KEYBOARD_BACKSLASH | KEY_MOD_LEFT_SHIFT;
    case ':': return HID_KEYBOARD_SEMICOLON | KEY_MOD_LEFT_SHIFT;
    case '"': return HID_KEYBOARD_APOSTROPHE | KEY_MOD_LEFT_SHIFT;
    case '~': return HID_KEYBOARD_GRAVE_ACCENT | KEY_MOD_LEFT_SHIFT;
    case '<': return HID_KEYBOARD_COMMA | KEY_MOD_LEFT_SHIFT;
    case '>': return HID_KEYBOARD_DOT | KEY_MOD_LEFT_SHIFT;
    case '?': return HID_KEYBOARD_SLASH | KEY_MOD_LEFT_SHIFT;

    default: return 0; // unsupported (e.g. accented chars) — skipped
    }
}

bool hid_type_string(const char* text, bool press_enter_after) {
    // Remember what USB was doing so we can restore it afterwards.
    FuriHalUsbInterface* prev = furi_hal_usb_get_config();

    furi_hal_usb_unlock();
    if(!furi_hal_usb_set_config(&usb_hid, NULL)) {
        FURI_LOG_E(FKM_TAG, "could not switch USB to HID");
        return false;
    }

    // Wait for the host to enumerate us.
    bool ready = false;
    for(uint32_t waited = 0; waited < HID_READY_TIMEOUT_MS; waited += 50) {
        if(furi_hal_hid_is_connected()) {
            ready = true;
            break;
        }
        furi_delay_ms(50);
    }

    if(ready) {
        for(const char* p = text; *p; p++) {
            uint16_t code = ascii_to_hid(*p);
            if(code == 0) continue; // skip chars we can't represent
            furi_hal_hid_kb_press(code);
            furi_delay_ms(HID_KEY_DELAY_MS);
            furi_hal_hid_kb_release(code);
            furi_delay_ms(HID_KEY_DELAY_MS);
        }
        if(press_enter_after) {
            furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
            furi_delay_ms(HID_KEY_DELAY_MS);
            furi_hal_hid_kb_release(HID_KEYBOARD_RETURN);
        }
    } else {
        FURI_LOG_W(FKM_TAG, "HID host not ready");
    }

    // Restore the previous USB personality (usually the VCP/CLI).
    furi_hal_usb_set_config(prev, NULL);
    return ready;
}

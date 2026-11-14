#include "hid_layout.h"

#include <furi.h>
#include <furi_hal_usb_hid.h>
#include <string.h>

#define FKM_TAG "FliiperKeys"

// US QWERTY mapping for a single ASCII char (0 = unsupported).
static uint16_t us_ascii_to_hid(char c) {
    if(c >= 'a' && c <= 'z') return HID_KEYBOARD_A + (c - 'a');
    if(c >= 'A' && c <= 'Z') return (HID_KEYBOARD_A + (c - 'A')) | KEY_MOD_LEFT_SHIFT;

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

    default: return 0;
    }
}

void hid_layout_load_us(uint16_t table[HID_LAYOUT_SIZE]) {
    for(int i = 0; i < HID_LAYOUT_SIZE; i++) {
        table[i] = us_ascii_to_hid((char)i);
    }
}

bool hid_layout_load_file(Storage* storage, const char* path, uint16_t table[HID_LAYOUT_SIZE]) {
    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // .kl files are little-endian uint16; read the first 128 entries.
        uint8_t raw[HID_LAYOUT_SIZE * 2];
        uint16_t got = storage_file_read(file, raw, sizeof(raw));
        if(got == sizeof(raw)) {
            for(int i = 0; i < HID_LAYOUT_SIZE; i++) {
                table[i] = (uint16_t)raw[i * 2] | ((uint16_t)raw[i * 2 + 1] << 8);
            }
            ok = true;
        } else {
            FURI_LOG_W(FKM_TAG, "layout file too short: %s", path);
        }
    } else {
        FURI_LOG_W(FKM_TAG, "cannot open layout: %s", path);
    }

    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

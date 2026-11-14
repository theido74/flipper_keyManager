#include "hid_typer.h"

#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

#define FKM_TAG "FliiperKeys"

// Wait up to this long for the host to enumerate the HID interface.
#define HID_READY_TIMEOUT_MS 2000
// Delay between key events; too fast and some hosts drop characters.
#define HID_KEY_DELAY_MS 8

bool hid_type_string(
    const uint16_t layout[HID_LAYOUT_SIZE],
    const char* text,
    bool press_enter_after) {
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
            uint8_t idx = (uint8_t)*p;
            if(idx >= HID_LAYOUT_SIZE) continue; // non-ASCII: can't map
            uint16_t code = layout[idx];
            if(code == 0) continue; // unsupported in this layout
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

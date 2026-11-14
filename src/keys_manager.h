#pragma once

// Fliiper Keys — application-wide types.
//
// Flow:
//   UiList (layout) -> TextInput (passphrase) -> UiList (domains) -> Widget (status)
// The vault file lives on the SD card at APP_DATA_PATH("vault.fkm"),
// is AES-256-CTR encrypted, and is decrypted in RAM only after the
// passphrase is entered. Selecting a domain types its password over
// USB HID, as if from a keyboard.

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>

#include "vault.h"
#include "hid_layout.h"
#include "ui_list.h"

#define FKM_TAG "FliiperKeys"

// Max passphrase length the text input will accept.
#define FKM_PASSPHRASE_MAX 64

// Max number of .kl layout files listed in the chooser.
#define FKM_MAX_LAYOUTS 32

// View identifiers for the ViewDispatcher.
typedef enum {
    FkmViewLayout, // submenu: pick the host keyboard layout
    FkmViewPassphrase, // text input
    FkmViewList, // submenu of domains
    FkmViewStatus, // widget: result / errors
} FkmView;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Storage* storage;

    TextInput* text_input;
    UiList* list; // animated domain list
    UiList* layout_menu; // animated keyboard-layout chooser
    Widget* widget;

    // Chosen ASCII->HID layout used when typing. Defaults to US.
    uint16_t layout[HID_LAYOUT_SIZE];

    // Absolute paths of discovered .kl layout files (NULL-padded).
    char* layout_paths[FKM_MAX_LAYOUTS];
    size_t layout_count;

    // Passphrase buffer, zeroed as soon as the key is derived.
    char passphrase[FKM_PASSPHRASE_MAX + 1];

    // Decrypted, parsed secrets. Lives only while the app is open.
    Vault* vault;
} KeysManagerApp;

// Shows the status view with a title + message, then returns to the list.
void keys_manager_show_status(KeysManagerApp* app, const char* title, const char* message);

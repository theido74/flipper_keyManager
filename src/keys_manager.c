#include "keys_manager.h"
#include "crypto.h"
#include "hid_typer.h"

#include <furi.h>
#include <storage/storage.h>

#define VAULT_PATH APP_DATA_PATH("vault.fkm")
#define FKM_VAULT_MAX_BYTES (64 * 1024) // refuse absurdly large files

// ---------------------------------------------------------------------------
// Status view helper
// ---------------------------------------------------------------------------
void keys_manager_show_status(KeysManagerApp* app, const char* title, const char* message) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 10, AlignCenter, AlignTop, FontPrimary, title);
    widget_add_string_multiline_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, message);
    view_dispatcher_switch_to_view(app->view_dispatcher, FkmViewStatus);
}

// ---------------------------------------------------------------------------
// Vault loading
// ---------------------------------------------------------------------------

// Reads the whole vault file into a malloc'd buffer. Returns false if the
// file is missing or too big. On success *data/*len are set (caller frees).
static bool read_vault_file(KeysManagerApp* app, uint8_t** data, size_t* len) {
    File* file = storage_file_alloc(app->storage);
    bool ok = false;

    do {
        if(!storage_file_open(file, VAULT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) break;

        uint64_t size = storage_file_size(file);
        if(size == 0 || size > FKM_VAULT_MAX_BYTES) break;

        uint8_t* buf = malloc((size_t)size);
        if(!buf) break;

        uint16_t chunk;
        size_t read_total = 0;
        while(read_total < size) {
            chunk = storage_file_read(file, buf + read_total, (uint16_t)(size - read_total));
            if(chunk == 0) break;
            read_total += chunk;
        }

        if(read_total != size) {
            free(buf);
            break;
        }

        *data = buf;
        *len = (size_t)size;
        ok = true;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

// Called once the passphrase is entered: read -> decrypt -> parse -> list.
static void submenu_select_callback(void* context, uint32_t index);

static void load_and_show_vault(KeysManagerApp* app) {
    uint8_t* file_data = NULL;
    size_t file_len = 0;

    if(!read_vault_file(app, &file_data, &file_len)) {
        keys_manager_show_status(
            app, "No vault", "Put vault.fkm in\napps_data/fliiper_keys");
        return;
    }

    uint8_t* plain = NULL;
    size_t plain_len = 0;
    bool decrypted = fkm_decrypt_file(app->passphrase, file_data, file_len, &plain, &plain_len);

    // Wipe the passphrase as soon as the key has been derived & used.
    memset(app->passphrase, 0, sizeof(app->passphrase));
    free(file_data);

    if(!decrypted) {
        keys_manager_show_status(app, "Error", "Bad file or\nwrong passphrase");
        return;
    }

    // Without a MAC we can't *prove* the passphrase was right; we infer it
    // from whether the plaintext parses into at least one entry.
    app->vault = vault_parse(plain, plain_len);
    memset(plain, 0, plain_len); // wipe decrypted plaintext copy
    free(plain);

    if(!app->vault || app->vault->count == 0) {
        keys_manager_show_status(app, "Empty / wrong", "No entries.\nWrong passphrase?");
        return;
    }

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select account");
    for(size_t i = 0; i < app->vault->count; i++) {
        submenu_add_item(
            app->submenu, app->vault->entries[i].domain, (uint32_t)i, submenu_select_callback, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, FkmViewList);
}

// ---------------------------------------------------------------------------
// Keyboard-layout chooser
// ---------------------------------------------------------------------------

static bool ends_with_kl(const char* name) {
    size_t n = strlen(name);
    return n > 3 && strcmp(name + n - 3, ".kl") == 0;
}

// Scan HID_LAYOUTS_DIR for *.kl files, storing absolute paths in app->layout_paths.
static void scan_layout_files(KeysManagerApp* app) {
    app->layout_count = 0;
    File* dir = storage_file_alloc(app->storage);

    if(storage_dir_open(dir, HID_LAYOUTS_DIR)) {
        FileInfo info;
        char name[128];
        while(app->layout_count < FKM_MAX_LAYOUTS &&
              storage_dir_read(dir, &info, name, sizeof(name))) {
            if((info.flags & FSF_DIRECTORY) || !ends_with_kl(name)) continue;

            size_t len = strlen(HID_LAYOUTS_DIR) + 1 + strlen(name) + 1;
            char* full = malloc(len);
            if(full) {
                snprintf(full, len, "%s/%s", HID_LAYOUTS_DIR, name);
                app->layout_paths[app->layout_count++] = full;
            }
        }
    } else {
        FURI_LOG_I(FKM_TAG, "no layouts dir (%s); US only", HID_LAYOUTS_DIR);
    }

    storage_dir_close(dir);
    storage_file_free(dir);
}

// index 0 = built-in US; 1.. = app->layout_paths[index-1]
static void layout_menu_select_callback(void* context, uint32_t index) {
    KeysManagerApp* app = context;

    if(index == 0) {
        hid_layout_load_us(app->layout);
    } else {
        const char* path = app->layout_paths[index - 1];
        if(!hid_layout_load_file(app->storage, path, app->layout)) {
            hid_layout_load_us(app->layout); // fall back if the file is bad
            FURI_LOG_W(FKM_TAG, "layout load failed, using US");
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, FkmViewPassphrase);
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

static void text_input_done_callback(void* context) {
    KeysManagerApp* app = context;
    load_and_show_vault(app);
}

static void submenu_select_callback(void* context, uint32_t index) {
    KeysManagerApp* app = context;
    if(!app->vault || index >= app->vault->count) return;

    const VaultEntry* entry = &app->vault->entries[index];

    keys_manager_show_status(app, entry->domain, "Typing over USB...");
    bool ok = hid_type_string(app->layout, entry->password, /*press_enter_after=*/false);

    keys_manager_show_status(
        app, entry->domain, ok ? "Typed." : "USB host\nnot ready");
}

// Back button handling: Status/List -> List/exit.
static bool navigation_callback(void* context) {
    KeysManagerApp* app = context;
    uint32_t current = view_dispatcher_get_current_view(app->view_dispatcher);

    if(current == FkmViewStatus && app->vault) {
        view_dispatcher_switch_to_view(app->view_dispatcher, FkmViewList);
        return true; // handled
    }
    if(current == FkmViewPassphrase) {
        view_dispatcher_switch_to_view(app->view_dispatcher, FkmViewLayout);
        return true;
    }
    // From the layout chooser, the list, or an error status, Back exits.
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------

static KeysManagerApp* keys_manager_app_alloc(void) {
    KeysManagerApp* app = malloc(sizeof(KeysManagerApp));
    memset(app, 0, sizeof(KeysManagerApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);

    app->text_input = text_input_alloc();
    text_input_set_header_text(app->text_input, "Vault passphrase");
    text_input_set_result_callback(
        app->text_input,
        text_input_done_callback,
        app,
        app->passphrase,
        FKM_PASSPHRASE_MAX,
        /*clear_default_text=*/true);

    app->submenu = submenu_alloc();
    app->layout_menu = submenu_alloc();
    app->widget = widget_alloc();

    // Default layout until the user picks one.
    hid_layout_load_us(app->layout);

    // Build the layout chooser: built-in US + any .kl files on the SD card.
    scan_layout_files(app);
    submenu_set_header(app->layout_menu, "Host keyboard layout");
    submenu_add_item(
        app->layout_menu, "Built-in US (QWERTY)", 0, layout_menu_select_callback, app);
    for(size_t i = 0; i < app->layout_count; i++) {
        // Show the file name (after the last '/'), e.g. "ch-fr.kl".
        const char* full = app->layout_paths[i];
        const char* base = strrchr(full, '/');
        base = base ? base + 1 : full;
        submenu_add_item(
            app->layout_menu, base, (uint32_t)(i + 1), layout_menu_select_callback, app);
    }

    view_dispatcher_add_view(
        app->view_dispatcher, FkmViewLayout, submenu_get_view(app->layout_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, FkmViewPassphrase, text_input_get_view(app->text_input));
    view_dispatcher_add_view(app->view_dispatcher, FkmViewList, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, FkmViewStatus, widget_get_view(app->widget));

    return app;
}

static void keys_manager_app_free(KeysManagerApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FkmViewLayout);
    view_dispatcher_remove_view(app->view_dispatcher, FkmViewPassphrase);
    view_dispatcher_remove_view(app->view_dispatcher, FkmViewList);
    view_dispatcher_remove_view(app->view_dispatcher, FkmViewStatus);

    text_input_free(app->text_input);
    submenu_free(app->submenu);
    submenu_free(app->layout_menu);
    widget_free(app->widget);
    view_dispatcher_free(app->view_dispatcher);

    for(size_t i = 0; i < app->layout_count; i++) {
        free(app->layout_paths[i]);
    }

    if(app->vault) vault_free(app->vault);
    memset(app->passphrase, 0, sizeof(app->passphrase));

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

// Entry point named in application.fam.
int32_t keys_manager_app(void* p) {
    UNUSED(p);
    KeysManagerApp* app = keys_manager_app_alloc();

    view_dispatcher_switch_to_view(app->view_dispatcher, FkmViewLayout);
    view_dispatcher_run(app->view_dispatcher);

    keys_manager_app_free(app);
    return 0;
}

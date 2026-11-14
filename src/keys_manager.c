#include "keys_manager.h"
#include "crypto.h"
#include "hid_typer.h"

#include <furi.h>
#include <storage/storage.h>

#define VAULT_PATH APP_DATA_PATH("vault.fkm")
#define FKM_VAULT_MAX_BYTES (64 * 1024) // refuse absurdly large files

// Custom events posted when a list's click animation completes. They are sent
// from the animation timer thread via view_dispatcher_send_custom_event (safe)
// and handled on the GUI thread in custom_event_callback.
typedef enum {
    FkmEvtActivateLayout = 1,
    FkmEvtActivateDomain,
} FkmCustomEvent;

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
static bool read_vault_file(KeysManagerApp* app, uint8_t** data, size_t* len) {
    File* file = storage_file_alloc(app->storage);
    bool ok = false;

    do {
        if(!storage_file_open(file, VAULT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) break;

        uint64_t size = storage_file_size(file);
        if(size == 0 || size > FKM_VAULT_MAX_BYTES) break;

        uint8_t* buf = malloc((size_t)size);
        if(!buf) break;

        size_t read_total = 0;
        while(read_total < size) {
            uint16_t chunk =
                storage_file_read(file, buf + read_total, (uint16_t)(size - read_total));
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

// Read -> decrypt -> parse -> fill the animated list.
static void load_and_show_vault(KeysManagerApp* app) {
    uint8_t* file_data = NULL;
    size_t file_len = 0;

    if(!read_vault_file(app, &file_data, &file_len)) {
        keys_manager_show_status(app, "No vault", "Put vault.fkm in\napps_data/fliiper_keys");
        return;
    }

    uint8_t* plain = NULL;
    size_t plain_len = 0;
    bool decrypted = fkm_decrypt_file(app->passphrase, file_data, file_len, &plain, &plain_len);

    memset(app->passphrase, 0, sizeof(app->passphrase));
    free(file_data);

    if(!decrypted) {
        keys_manager_show_status(app, "Error", "Bad file or\nwrong passphrase");
        return;
    }

    app->vault = vault_parse(plain, plain_len);
    memset(plain, 0, plain_len);
    free(plain);

    if(!app->vault || app->vault->count == 0) {
        keys_manager_show_status(app, "Empty / wrong", "No entries.\nWrong passphrase?");
        return;
    }

    ui_list_reset(app->list);
    ui_list_set_header(app->list, "Accounts");
    for(size_t i = 0; i < app->vault->count; i++) {
        ui_list_add_item(app->list, app->vault->entries[i].domain);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, FkmViewList);
}

// ---------------------------------------------------------------------------
// Keyboard-layout discovery
// ---------------------------------------------------------------------------
static bool ends_with_kl(const char* name) {
    size_t n = strlen(name);
    return n > 3 && strcmp(name + n - 3, ".kl") == 0;
}

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

// ---------------------------------------------------------------------------
// Activation (runs on the GUI thread, posted by the list animations)
// ---------------------------------------------------------------------------
static bool custom_event_callback(void* context, uint32_t event) {
    KeysManagerApp* app = context;

    switch(event) {
    case FkmEvtActivateLayout: {
        size_t idx = ui_list_get_selected(app->layout_menu);
        if(idx == 0) {
            hid_layout_load_us(app->layout);
        } else if(idx - 1 < app->layout_count) {
            const char* path = app->layout_paths[idx - 1];
            if(!hid_layout_load_file(app->storage, path, app->layout)) {
                hid_layout_load_us(app->layout);
                FURI_LOG_W(FKM_TAG, "layout load failed, using US");
            }
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FkmViewPassphrase);
        return true;
    }

    case FkmEvtActivateDomain: {
        if(!app->vault) return true;
        size_t idx = ui_list_get_selected(app->list);
        if(idx >= app->vault->count) return true;

        const VaultEntry* entry = &app->vault->entries[idx];
        bool ok = hid_type_string(app->layout, entry->password, /*press_enter_after=*/false);
        keys_manager_show_status(app, entry->domain, ok ? "Typed." : "USB host\nnot ready");
        return true;
    }

    default:
        return false;
    }
}

static void list_done_callback(void* context) {
    KeysManagerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FkmEvtActivateDomain);
}

static void layout_done_callback(void* context) {
    KeysManagerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FkmEvtActivateLayout);
}

static void text_input_done_callback(void* context) {
    KeysManagerApp* app = context;
    load_and_show_vault(app);
}

// Back-button handling.
static bool navigation_callback(void* context) {
    KeysManagerApp* app = context;
    uint32_t current = view_dispatcher_get_current_view(app->view_dispatcher);

    if(current == FkmViewStatus && app->vault) {
        view_dispatcher_switch_to_view(app->view_dispatcher, FkmViewList);
        return true;
    }
    if(current == FkmViewPassphrase) {
        view_dispatcher_switch_to_view(app->view_dispatcher, FkmViewLayout);
        return true;
    }
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
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);

    app->text_input = text_input_alloc();
    text_input_set_header_text(app->text_input, "Vault passphrase");
    text_input_set_result_callback(
        app->text_input,
        text_input_done_callback,
        app,
        app->passphrase,
        FKM_PASSPHRASE_MAX,
        /*clear_default_text=*/true);

    app->list = ui_list_alloc();
    ui_list_set_done(app->list, list_done_callback, app);

    app->layout_menu = ui_list_alloc();
    ui_list_set_done(app->layout_menu, layout_done_callback, app);

    app->widget = widget_alloc();

    // Default layout until the user picks one.
    hid_layout_load_us(app->layout);

    // Build the layout chooser: built-in US + any .kl files on the SD card.
    scan_layout_files(app);
    ui_list_set_header(app->layout_menu, "Layout");
    ui_list_add_item(app->layout_menu, "Built-in US (QWERTY)");
    for(size_t i = 0; i < app->layout_count; i++) {
        const char* full = app->layout_paths[i];
        const char* base = strrchr(full, '/');
        ui_list_add_item(app->layout_menu, base ? base + 1 : full);
    }

    view_dispatcher_add_view(app->view_dispatcher, FkmViewLayout, ui_list_get_view(app->layout_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, FkmViewPassphrase, text_input_get_view(app->text_input));
    view_dispatcher_add_view(app->view_dispatcher, FkmViewList, ui_list_get_view(app->list));
    view_dispatcher_add_view(app->view_dispatcher, FkmViewStatus, widget_get_view(app->widget));

    return app;
}

static void keys_manager_app_free(KeysManagerApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FkmViewLayout);
    view_dispatcher_remove_view(app->view_dispatcher, FkmViewPassphrase);
    view_dispatcher_remove_view(app->view_dispatcher, FkmViewList);
    view_dispatcher_remove_view(app->view_dispatcher, FkmViewStatus);

    text_input_free(app->text_input);
    ui_list_free(app->list);
    ui_list_free(app->layout_menu);
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

int32_t keys_manager_app(void* p) {
    UNUSED(p);
    KeysManagerApp* app = keys_manager_app_alloc();

    view_dispatcher_switch_to_view(app->view_dispatcher, FkmViewLayout);
    view_dispatcher_run(app->view_dispatcher);

    keys_manager_app_free(app);
    return 0;
}

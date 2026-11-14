#pragma once

// UiList — a custom, animated list view for Flipper (128x64, 1-bit).
//
// Visual design:
//   - a black header bar with a little key mark, the title, and an "i/N" count
//   - rows with a rounded highlight that *slides* between items as you scroll
//   - a slim right-hand scrollbar when the list overflows
//
// Click animation:
//   - pressing OK fills the selected row and emits an expanding "ripple"
//     frame; when the short animation ends, the `done` callback fires (from
//     the animation timer thread, so `done` must only post an event — e.g.
//     view_dispatcher_send_custom_event — never touch the GUI directly).
//
// It replaces the stock Submenu for both the layout chooser and the vault
// list. Item labels are copied in.

#include <gui/view.h>
#include <stddef.h>

typedef struct UiList UiList;

// Fires when a click animation completes. Post an event here; do not block.
typedef void (*UiListDone)(void* context);

UiList* ui_list_alloc(void);
void ui_list_free(UiList* list);
View* ui_list_get_view(UiList* list);

// `done` is called (on the timer thread) when the OK-press animation ends.
void ui_list_set_done(UiList* list, UiListDone done, void* context);

void ui_list_set_header(UiList* list, const char* header);
void ui_list_reset(UiList* list); // clears items + selection
void ui_list_add_item(UiList* list, const char* label);

// Index of the currently highlighted item (what OK acted on).
size_t ui_list_get_selected(UiList* list);

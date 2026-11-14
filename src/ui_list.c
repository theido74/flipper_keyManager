#include "ui_list.h"

#include <furi.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <string.h>
#include <stdlib.h>

#define UIL_W 128
#define UIL_H 64
#define UIL_HEADER_H 14
#define UIL_ROW_H 12
#define UIL_VIS 4 // (64-14)/12 -> 4 visible rows
#define UIL_FRAME_MS 30 // ~33 fps animation tick
#define UIL_CLICK_FRAMES 8 // length of the OK-press animation

typedef struct {
    char** items; // owned copies
    size_t count;
    char* header; // owned copy
    size_t selected;
    size_t window_top; // first visible row
    float sel_y; // animated highlight Y (eased toward target)
    uint8_t click_anim; // >0 while the press animation plays
} UiListModel;

struct UiList {
    View* view;
    FuriTimer* timer;
    UiListDone done;
    void* done_ctx;
};

// Keep `selected` visible; return the adjusted window top.
static size_t clamp_window(size_t selected, size_t count, size_t window) {
    if(count <= UIL_VIS) return 0;
    if(selected < window) window = selected;
    if(selected >= window + UIL_VIS) window = selected - UIL_VIS + 1;
    if(window + UIL_VIS > count) window = count - UIL_VIS;
    return window;
}

static float target_y(size_t selected, size_t window_top) {
    return (float)UIL_HEADER_H + (float)(selected - window_top) * UIL_ROW_H;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void ui_list_draw(Canvas* canvas, void* _model) {
    UiListModel* m = _model;
    canvas_clear(canvas);

    m->window_top = clamp_window(m->selected, m->count, m->window_top);

    bool has_bar = m->count > UIL_VIS;
    int right_margin = has_bar ? 6 : 3;
    int box_w = UIL_W - 2 - right_margin;

    // --- Rows (drawn first; highlight paints over the selected one) ---
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorBlack);
    for(size_t i = m->window_top; i < m->window_top + UIL_VIS && i < m->count; i++) {
        int y = UIL_HEADER_H + (int)(i - m->window_top) * UIL_ROW_H;
        if(i != m->selected) {
            canvas_draw_str(canvas, 6, y + UIL_ROW_H - 3, m->items[i]);
        }
    }

    // --- Animated highlight on the selected row ---
    if(m->count > 0) {
        int by = (int)(m->sel_y + 0.5f);

        // Expanding ripple while the click animation plays.
        if(m->click_anim > 0) {
            int off = (UIL_CLICK_FRAMES - m->click_anim); // grows 0..7
            canvas_draw_rframe(
                canvas, 2 - off, by - off, box_w + off * 2, UIL_ROW_H - 1 + off * 2, 2);
        }

        // Filled rounded highlight + inverted label.
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, 2, by, box_w, UIL_ROW_H - 1, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 6, by + UIL_ROW_H - 3, m->items[m->selected]);
        canvas_set_color(canvas, ColorBlack);
    }

    // --- Scrollbar ---
    if(has_bar) {
        int tx = UIL_W - 3;
        int ty = UIL_HEADER_H;
        int th = UIL_H - UIL_HEADER_H;
        canvas_draw_line(canvas, tx, ty, tx, UIL_H - 1);
        int thumb_h = th * UIL_VIS / (int)m->count;
        if(thumb_h < 4) thumb_h = 4;
        int span = th - thumb_h;
        int thumb_y = ty + (int)(span * m->window_top / (m->count - UIL_VIS));
        canvas_draw_box(canvas, tx - 1, thumb_y, 3, thumb_h);
    }

    // --- Header bar (on top) ---
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, UIL_W, UIL_HEADER_H);
    canvas_set_color(canvas, ColorWhite);

    // little key mark
    canvas_draw_circle(canvas, 5, 7, 2);
    canvas_draw_line(canvas, 7, 7, 10, 7);
    canvas_draw_line(canvas, 9, 7, 9, 9);

    canvas_set_font(canvas, FontPrimary);
    if(m->header) canvas_draw_str(canvas, 15, UIL_HEADER_H - 4, m->header);

    if(m->count > 0) {
        char counter[12];
        snprintf(counter, sizeof(counter), "%u/%u", (unsigned)(m->selected + 1), (unsigned)m->count);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, UIL_W - 2, UIL_HEADER_H - 4, AlignRight, AlignBottom, counter);
    }
    canvas_set_color(canvas, ColorBlack);
}

// ---------------------------------------------------------------------------
// Animation tick
// ---------------------------------------------------------------------------
static void ui_list_tick(void* ctx) {
    UiList* l = ctx;
    bool redraw = false;
    bool fire = false;

    with_view_model(
        l->view,
        UiListModel * m,
        {
            float t = target_y(m->selected, m->window_top);
            float d = t - m->sel_y;
            if(d < 0) d = -d;
            if(d > 0.5f) {
                m->sel_y += (t - m->sel_y) * 0.4f;
                redraw = true;
            } else if(m->sel_y != t) {
                m->sel_y = t;
                redraw = true;
            }
            if(m->click_anim > 0) {
                m->click_anim--;
                redraw = true;
                if(m->click_anim == 0) fire = true;
            }
        },
        redraw);

    if(fire && l->done) l->done(l->done_ctx);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
static bool ui_list_input(InputEvent* event, void* ctx) {
    UiList* l = ctx;
    bool consumed = false;

    bool press = event->type == InputTypeShort || event->type == InputTypeRepeat;
    if(!press) return false;

    with_view_model(
        l->view,
        UiListModel * m,
        {
            if(m->click_anim > 0) {
                consumed = true; // ignore input mid-animation
            } else if(event->key == InputKeyUp) {
                if(m->selected > 0) m->selected--;
                m->window_top = clamp_window(m->selected, m->count, m->window_top);
                consumed = true;
            } else if(event->key == InputKeyDown) {
                if(m->selected + 1 < m->count) m->selected++;
                m->window_top = clamp_window(m->selected, m->count, m->window_top);
                consumed = true;
            } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
                if(m->count > 0) m->click_anim = UIL_CLICK_FRAMES;
                consumed = true;
            }
        },
        consumed);

    return consumed; // Back (and others) fall through to the navigation callback
}

// ---------------------------------------------------------------------------
// Enter / exit drive the animation timer (saves power when hidden)
// ---------------------------------------------------------------------------
static void ui_list_enter(void* ctx) {
    UiList* l = ctx;
    with_view_model(
        l->view,
        UiListModel * m,
        {
            m->window_top = clamp_window(m->selected, m->count, m->window_top);
            m->sel_y = target_y(m->selected, m->window_top);
            m->click_anim = 0;
        },
        true);
    furi_timer_start(l->timer, furi_ms_to_ticks(UIL_FRAME_MS));
}

static void ui_list_exit(void* ctx) {
    UiList* l = ctx;
    furi_timer_stop(l->timer);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
UiList* ui_list_alloc(void) {
    UiList* l = malloc(sizeof(UiList));
    memset(l, 0, sizeof(UiList));

    l->view = view_alloc();
    view_set_context(l->view, l);
    view_allocate_model(l->view, ViewModelTypeLocking, sizeof(UiListModel));
    view_set_draw_callback(l->view, ui_list_draw);
    view_set_input_callback(l->view, ui_list_input);
    view_set_enter_callback(l->view, ui_list_enter);
    view_set_exit_callback(l->view, ui_list_exit);

    with_view_model(
        l->view,
        UiListModel * m,
        {
            m->items = NULL;
            m->count = 0;
            m->header = NULL;
            m->selected = 0;
            m->window_top = 0;
            m->sel_y = UIL_HEADER_H;
            m->click_anim = 0;
        },
        false);

    l->timer = furi_timer_alloc(ui_list_tick, FuriTimerTypePeriodic, l);
    return l;
}

static void free_items(UiListModel* m) {
    for(size_t i = 0; i < m->count; i++) free(m->items[i]);
    free(m->items);
    m->items = NULL;
    m->count = 0;
}

void ui_list_free(UiList* list) {
    furi_timer_stop(list->timer);
    furi_timer_free(list->timer);
    with_view_model(
        list->view,
        UiListModel * m,
        {
            free_items(m);
            free(m->header);
            m->header = NULL;
        },
        false);
    view_free(list->view);
    free(list);
}

View* ui_list_get_view(UiList* list) {
    return list->view;
}

void ui_list_set_done(UiList* list, UiListDone done, void* context) {
    list->done = done;
    list->done_ctx = context;
}

void ui_list_set_header(UiList* list, const char* header) {
    with_view_model(
        list->view,
        UiListModel * m,
        {
            free(m->header);
            m->header = header ? strdup(header) : NULL;
        },
        true);
}

void ui_list_reset(UiList* list) {
    with_view_model(
        list->view,
        UiListModel * m,
        {
            free_items(m);
            m->selected = 0;
            m->window_top = 0;
            m->sel_y = UIL_HEADER_H;
            m->click_anim = 0;
        },
        true);
}

void ui_list_add_item(UiList* list, const char* label) {
    with_view_model(
        list->view,
        UiListModel * m,
        {
            char** grown = realloc(m->items, sizeof(char*) * (m->count + 1));
            if(grown) {
                m->items = grown;
                m->items[m->count] = strdup(label);
                if(m->items[m->count]) m->count++;
            }
        },
        true);
}

size_t ui_list_get_selected(UiList* list) {
    size_t sel = 0;
    with_view_model(
        list->view, UiListModel * m, { sel = m->selected; }, false);
    return sel;
}

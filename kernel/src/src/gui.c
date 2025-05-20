#include "gui.h"
#include <stddef.h>

void gui_init(struct gui_context *ctx, struct Framebuffer fb) {
    if (!ctx) return;
    ctx->fb = (uint32_t*)fb.base_address;
    ctx->width = fb.width;
    ctx->height = fb.height;
    ctx->pitch = fb.pixels_per_scan_line;
}

void gui_fill_rect(struct gui_context *ctx, int x, int y, int w, int h, uint32_t color) {
    if (!ctx || !ctx->fb) return;
    for (int j = 0; j < h; j++) {
        if ((unsigned)(y + j) >= ctx->height) break;
        uint32_t *row = ctx->fb + (y + j) * ctx->pitch;
        for (int i = 0; i < w; i++) {
            if ((unsigned)(x + i) >= ctx->width) break;
            row[x + i] = color;
        }
    }
}

void gui_draw_window(struct gui_context *ctx, int x, int y, int w, int h,
                     uint32_t bg_color, uint32_t border_color) {
    if (w <= 2 || h <= 2) return;
    gui_fill_rect(ctx, x, y, w, 1, border_color);
    gui_fill_rect(ctx, x, y + h - 1, w, 1, border_color);
    gui_fill_rect(ctx, x, y, 1, h, border_color);
    gui_fill_rect(ctx, x + w - 1, y, 1, h, border_color);
    gui_fill_rect(ctx, x + 1, y + 1, w - 2, h - 2, bg_color);
}

void gui_draw_desktop(struct gui_context *ctx) {
    if (!ctx) return;
    gui_fill_rect(ctx, 0, 0, ctx->width, ctx->height, 0x002244);
    gui_draw_window(ctx, 50, 50, ctx->width / 2, ctx->height / 2, 0xcccccc, 0x000000);
}

void gui_draw_cursor(struct gui_context *ctx, int x, int y, uint32_t color) {
    gui_fill_rect(ctx, x, y, 10, 1, color);
    gui_fill_rect(ctx, x, y, 1, 10, color);
}

void gui_draw_window_ex(struct gui_context *ctx, struct gui_window *win,
                        uint32_t bg_color, uint32_t border_color) {
    if (!win || win->state == GUI_WINDOW_CLOSED)
        return;

    int x = win->x;
    int y = win->y;
    int w = win->w;
    int h = win->h;

    if (win->state == GUI_WINDOW_MAXIMIZED) {
        x = 0;
        y = 0;
        w = ctx->width;
        h = ctx->height;
    } else if (win->state == GUI_WINDOW_MINIMIZED) {
        h = 20;
    }

    gui_draw_window(ctx, x, y, w, h, bg_color, border_color);
    gui_fill_rect(ctx, x + w - 45, y + 2, 12, 12, 0x666666); // min
    gui_fill_rect(ctx, x + w - 30, y + 2, 12, 12, 0x666666); // max
    gui_fill_rect(ctx, x + w - 15, y + 2, 12, 12, 0x666666); // close
}

bool gui_window_handle_click(struct gui_window *win, int x, int y) {
    if (!win || win->state == GUI_WINDOW_CLOSED)
        return false;

    if (x < win->x || x >= win->x + win->w ||
        y < win->y || y >= win->y + 20)
        return false;

    int rel = x - win->x;
    if (rel >= win->w - 45 && rel < win->w - 30) {
        win->state = GUI_WINDOW_MINIMIZED;
        return true;
    }
    if (rel >= win->w - 30 && rel < win->w - 15) {
        win->state = (win->state == GUI_WINDOW_MAXIMIZED) ? GUI_WINDOW_NORMAL : GUI_WINDOW_MAXIMIZED;
        return true;
    }
    if (rel >= win->w - 15) {
        win->state = GUI_WINDOW_CLOSED;
        return true;
    }

    return false;
}

// Simple demo loop using the mouse driver
#include "mouse.h"

void gui_run_demo(struct gui_context *ctx) {
    if (!ctx) return;
    mouse_init();
    struct gui_window win = {50, 50, ctx->width / 2, ctx->height / 2, GUI_WINDOW_NORMAL};

    for (;;) {
        mouse_poll();
        struct mouse_state *ms = mouse_get_state();

        gui_draw_desktop(ctx);
        gui_draw_window_ex(ctx, &win, 0xcccccc, 0x000000);
        gui_draw_cursor(ctx, ms->x, ms->y, 0xffffff);

        if (ms->buttons & 1) {
            if (gui_window_handle_click(&win, ms->x, ms->y)) {
                while (mouse_get_state()->buttons & 1)
                    mouse_poll();
            }
        }
    }
}



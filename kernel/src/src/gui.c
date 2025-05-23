#include "gui.h"
#include <stddef.h>
#include <string.h>

#define GUI_MAX_WIDTH 1920
#define GUI_MAX_HEIGHT 1200

static uint32_t gui_backbuffer[GUI_MAX_WIDTH * GUI_MAX_HEIGHT];

void gui_init(struct gui_context *ctx, struct Framebuffer fb) {
    if (!ctx) return;
    ctx->fb = (uint32_t*)fb.base_address;
    ctx->width = fb.width;
    ctx->height = fb.height;
    ctx->pitch = fb.pixels_per_scan_line;
    ctx->backbuffer = gui_backbuffer;
}

void gui_fill_rect(struct gui_context *ctx, int x, int y, int w, int h, uint32_t color) {
    if (!ctx || !ctx->backbuffer) return;
    for (int j = 0; j < h; j++) {
        if ((unsigned)(y + j) >= ctx->height) break;
        uint32_t *row = ctx->backbuffer + (y + j) * ctx->width;
        for (int i = 0; i < w; i++) {
            if ((unsigned)(x + i) >= ctx->width) break;
            row[x + i] = color;
        }
    }
}

void gui_draw_window(struct gui_context *ctx, int x, int y, int w, int h,
                     uint32_t bg_color, uint32_t border_color, int radius) {
    if (w <= 2 || h <= 2) return;

    if (radius <= 0) {
        gui_fill_rect(ctx, x, y, w, 1, border_color);
        gui_fill_rect(ctx, x, y + h - 1, w, 1, border_color);
        gui_fill_rect(ctx, x, y, 1, h, border_color);
        gui_fill_rect(ctx, x + w - 1, y, 1, h, border_color);
        gui_fill_rect(ctx, x + 1, y + 1, w - 2, h - 2, bg_color);
        return;
    }

    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;

    gui_fill_rect(ctx, x + radius, y, w - 2 * radius, radius, border_color);
    gui_fill_rect(ctx, x + radius, y + h - radius, w - 2 * radius, radius, border_color);
    gui_fill_rect(ctx, x, y + radius, radius, h - 2 * radius, border_color);
    gui_fill_rect(ctx, x + w - radius, y + radius, radius, h - 2 * radius, border_color);

    gui_fill_rect(ctx, x + radius, y + radius, w - 2 * radius, h - 2 * radius, bg_color);

    int r2 = radius * radius;
    for (int j = 0; j < radius; j++) {
        for (int i = 0; i < radius; i++) {
            int dx = radius - 1 - i;
            int dy = radius - 1 - j;
            if (dx * dx + dy * dy >= r2) {
                gui_fill_rect(ctx, x + i, y + j, 1, 1, border_color);
                gui_fill_rect(ctx, x + w - 1 - i, y + j, 1, 1, border_color);
                gui_fill_rect(ctx, x + i, y + h - 1 - j, 1, 1, border_color);
                gui_fill_rect(ctx, x + w - 1 - i, y + h - 1 - j, 1, 1, border_color);
            } else {
                gui_fill_rect(ctx, x + i, y + j, 1, 1, bg_color);
                gui_fill_rect(ctx, x + w - 1 - i, y + j, 1, 1, bg_color);
                gui_fill_rect(ctx, x + i, y + h - 1 - j, 1, 1, bg_color);
                gui_fill_rect(ctx, x + w - 1 - i, y + h - 1 - j, 1, 1, bg_color);
            }
        }
    }
}

void gui_draw_desktop(struct gui_context *ctx) {
    if (!ctx) return;
    gui_fill_rect(ctx, 0, 0, ctx->width, ctx->height, 0x002244);
    gui_draw_window(ctx, 50, 50, ctx->width / 2, ctx->height / 2,
                    0xcccccc, 0x000000, 8);
}

void gui_draw_cursor(struct gui_context *ctx, int x, int y, uint32_t color) {
    static const uint8_t arrow[8] = {
        0b10000000,
        0b11000000,
        0b10100000,
        0b10010000,
        0b10001000,
        0b10000100,
        0b10000010,
        0b11111111,
    };

    for (int j = 0; j < 8; j++) {
        for (int i = 0; i < 8; i++) {
            if (arrow[j] & (0x80 >> i))
                gui_fill_rect(ctx, x + i, y + j, 1, 1, color);
        }
    }
}

void gui_flush(struct gui_context *ctx) {
    if (!ctx || !ctx->fb || !ctx->backbuffer) return;
    for (unsigned int j = 0; j < ctx->height; j++) {
        memcpy(ctx->fb + j * ctx->pitch,
               ctx->backbuffer + j * ctx->width,
               ctx->width * sizeof(uint32_t));
    }
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

    gui_draw_window(ctx, x, y, w, h, bg_color, border_color, 8);
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

        if (ms->x < 0) ms->x = 0;
        if (ms->y < 0) ms->y = 0;
        if (ms->x >= (int)ctx->width)  ms->x = ctx->width - 1;
        if (ms->y >= (int)ctx->height) ms->y = ctx->height - 1;

        gui_draw_desktop(ctx);
        gui_draw_window_ex(ctx, &win, 0xcccccc, 0x000000);
        gui_draw_cursor(ctx, ms->x, ms->y, 0xffffff);
        gui_flush(ctx);

        if (ms->buttons & 1) {
            if (gui_window_handle_click(&win, ms->x, ms->y)) {
                while (mouse_get_state()->buttons & 1)
                    mouse_poll();
            }
        }
    }
}



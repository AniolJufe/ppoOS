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


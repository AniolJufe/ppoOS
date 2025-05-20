#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include "kernel.h"

struct gui_context {
    uint32_t *fb;
    unsigned int width;
    unsigned int height;
    unsigned int pitch;
};

void gui_init(struct gui_context *ctx, struct Framebuffer fb);
void gui_fill_rect(struct gui_context *ctx, int x, int y, int w, int h, uint32_t color);
void gui_draw_window(struct gui_context *ctx, int x, int y, int w, int h,
                     uint32_t bg_color, uint32_t border_color);
void gui_draw_desktop(struct gui_context *ctx);

enum gui_window_state {
    GUI_WINDOW_NORMAL,
    GUI_WINDOW_MINIMIZED,
    GUI_WINDOW_MAXIMIZED,
    GUI_WINDOW_CLOSED
};

struct gui_window {
    int x;
    int y;
    int w;
    int h;
    enum gui_window_state state;
};

void gui_draw_window_ex(struct gui_context *ctx, struct gui_window *win,
                        uint32_t bg_color, uint32_t border_color);
bool gui_window_handle_click(struct gui_window *win, int x, int y);
void gui_draw_cursor(struct gui_context *ctx, int x, int y, uint32_t color);
void gui_run_demo(struct gui_context *ctx);

#endif // GUI_H

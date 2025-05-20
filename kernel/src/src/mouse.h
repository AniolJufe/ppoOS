#pragma once
#include <stdint.h>

struct mouse_state {
    int x;
    int y;
    int dx;
    int dy;
    uint8_t buttons;
};

void mouse_init(void);
void mouse_poll(void);
struct mouse_state *mouse_get_state(void);

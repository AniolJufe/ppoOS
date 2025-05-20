#include "mouse.h"
#include <stdint.h>

#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static void mouse_wait_input(void) {
    while (inb(PS2_STATUS_PORT) & 0x02);
}

static void mouse_wait_output(void) {
    while (!(inb(PS2_STATUS_PORT) & 0x01));
}

static void mouse_write(uint8_t val) {
    mouse_wait_input();
    outb(PS2_COMMAND_PORT, 0xD4);
    mouse_wait_input();
    outb(PS2_DATA_PORT, val);
}

static uint8_t mouse_read(void) {
    mouse_wait_output();
    return inb(PS2_DATA_PORT);
}

static struct mouse_state ms;
static int8_t packet[3];
static int packet_cycle = 0;

void mouse_init(void) {
    ms.x = ms.y = ms.dx = ms.dy = 0;
    ms.buttons = 0;

    mouse_wait_input();
    outb(PS2_COMMAND_PORT, 0xA8); // enable auxiliary device
    mouse_wait_input();
    outb(PS2_COMMAND_PORT, 0x20); // get command byte
    mouse_wait_output();
    uint8_t status = inb(PS2_DATA_PORT);
    status |= 2; // enable interrupt from mouse
    mouse_wait_input();
    outb(PS2_COMMAND_PORT, 0x60); // set command byte
    mouse_wait_input();
    outb(PS2_DATA_PORT, status);

    // default settings
    mouse_write(0xF6); mouse_read();
    // enable packet streaming
    mouse_write(0xF4); mouse_read();
}

struct mouse_state *mouse_get_state(void) {
    return &ms;
}

void mouse_poll(void) {
    if (!(inb(PS2_STATUS_PORT) & 1))
        return;

    int8_t data = inb(PS2_DATA_PORT);
    packet[packet_cycle++] = data;
    if (packet_cycle < 3)
        return;
    packet_cycle = 0;

    ms.buttons = packet[0] & 0x07;
    int dx = packet[1];
    int dy = -packet[2];
    ms.dx = dx;
    ms.dy = dy;
    ms.x += dx;
    ms.y += dy;
    if (ms.x < 0) ms.x = 0;
    if (ms.y < 0) ms.y = 0;
}

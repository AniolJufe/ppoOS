#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "keyboard.h"

// Basic PS/2 keyboard polling for x86_64
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Scancode definitions
#define SCANCODE_CTRL  0x1D
#define SCANCODE_LSHIFT 0x2A
#define SCANCODE_RSHIFT 0x36
#define SCANCODE_ALT   0x38

// Key state tracking
static struct {
    bool ctrl_down;
    bool shift_down;
    bool alt_down;
} key_state = {0};

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline int keyboard_has_data(void) {
    // Bit 0 of status port is set if output buffer is full
    return inb(KEYBOARD_STATUS_PORT) & 1;
}

// Simple US QWERTY scancode set 1 to ASCII
static const char scancode_set1[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t', 'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0,
    // F1-F10, etc
};

// US QWERTY scancode set 1 to ASCII (shift version)
static const char scancode_set1_shift[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t', 'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0,'|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' ', 0,
    // F1-F10, etc
};

char keyboard_read_char(void) {
    while (!keyboard_has_data());
    uint8_t sc = inb(KEYBOARD_DATA_PORT);
    
    // Handle key release (bit 7 set)
    if (sc & 0x80) {
        sc &= 0x7F; // Clear bit 7 to get the actual scancode
        
        // Update modifier key states on release
        if (sc == SCANCODE_CTRL) {
            key_state.ctrl_down = false;
        } else if (sc == SCANCODE_LSHIFT || sc == SCANCODE_RSHIFT) {
            key_state.shift_down = false;
        } else if (sc == SCANCODE_ALT) {
            key_state.alt_down = false;
        }
        
        return 0; // ignore key releases for normal keys
    }
    
    // Handle key press
    
    // Update modifier key states on press
    if (sc == SCANCODE_CTRL) {
        key_state.ctrl_down = true;
        return 0; // Don't return a character for modifier key presses
    } else if (sc == SCANCODE_LSHIFT || sc == SCANCODE_RSHIFT) {
        key_state.shift_down = true;
        return 0; // Don't return a character for modifier key presses
    } else if (sc == SCANCODE_ALT) {
        key_state.alt_down = true;
        return 0; // Don't return a character for modifier key presses
    }
    
    // Handle normal keys
    char c;
    if (key_state.shift_down) {
        c = scancode_set1_shift[sc];
    } else {
        c = scancode_set1[sc];
    }
    
    // Handle control combinations
    if (key_state.ctrl_down && c >= 'a' && c <= 'z') {
        // Convert to control code (ASCII 1-26)
        return c - 'a' + 1;
    }
    
    return c;
}

bool keyboard_ctrl_pressed(void) {
    return key_state.ctrl_down;
}

bool keyboard_shift_pressed(void) {
    return key_state.shift_down;
}

bool keyboard_alt_pressed(void) {
    return key_state.alt_down;
}

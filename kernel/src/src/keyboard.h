#pragma once
#include <stdbool.h>

// Read a character from the keyboard
char keyboard_read_char(void);

// Check if modifier keys are pressed
bool keyboard_ctrl_pressed(void);
bool keyboard_shift_pressed(void);
bool keyboard_alt_pressed(void);

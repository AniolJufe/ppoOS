#pragma once
#include <stdint.h>

// This symbol should be defined in your linker script to point to the top of your kernel stack
extern uint8_t kernel_stack_top[];

#ifndef STATE_PLATFORM_H
#define STATE_PLATFORM_H

#include <gb/gb.h>

void platform_init(void) BANKED;
void platform_update(void) BANKED;
void rope_trigger_enter(UWORD anchor_x, UWORD anchor_y, UBYTE block_length, UBYTE max_angle_degrees) BANKED;

#endif

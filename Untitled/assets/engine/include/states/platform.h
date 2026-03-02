#ifndef STATE_PLATFORM_H
#define STATE_PLATFORM_H

#include <gb/gb.h>

typedef enum {
    FALL_STATE = 0,
    GROUND_STATE,
    JUMP_STATE,
    DASH_STATE,
    LADDER_STATE,
    WALL_STATE,
    KNOCKBACK_STATE,
    BLANK_STATE,
    RUN_STATE,
    FLOAT_STATE,
    ROPE_STATE,
} state_e;

extern state_e plat_state;
extern state_e plat_next_state;

void platform_init(void) BANKED;
void platform_update(void) BANKED;
void rope_trigger_enter(UWORD anchor_x, UWORD anchor_y, UBYTE block_length, UBYTE max_angle_degrees, UBYTE swing_speed) BANKED;

#endif

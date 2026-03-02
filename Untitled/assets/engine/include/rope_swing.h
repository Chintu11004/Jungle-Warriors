#pragma once
#include "vm.h"

#define ROPE_LENGTH_TILES 5
#define ROPE_ANGLE 32 // 45 degree index in sinwave array

BANKREF_EXTERN(ROPE_SWING)
void rope_swing_enter(SCRIPT_CTX *THIS, INT16 anchor_x, INT16 anchor_y, INT16 length, INT16 max_angle) OLDCALL BANKED;
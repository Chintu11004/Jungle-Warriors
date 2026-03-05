#pragma bank 255
#include "vm.h"
#include "states/platform.h"
#include "rope_swing.h"
#include "math.h"

BANKREF(ROPE_SWING)

/*
    rope_swing_update — called by VM_INVOKE every frame.

    stack_frame layout (PARAMS = .ARG4 = -5, so stack_frame[0] = first pushed):
        stack_frame[0] = anchor_x       (first pushed, deepest)
        stack_frame[1] = anchor_y
        stack_frame[2] = length
        stack_frame[3] = max_angle
        stack_frame[4] = speed_factor   (last pushed, top = ARG0)

    On start == TRUE : initialise the rope state.
    Each frame       : yield (waitable = 1) and return FALSE while still swinging.
    Returns TRUE     : once the player has left ROPE_STATE — VM then pops the 5 stack slots.
*/
UBYTE rope_swing_update(void *THIS_void, UBYTE start, UWORD *stack_frame) OLDCALL BANKED {
    SCRIPT_CTX *THIS = (SCRIPT_CTX *)THIS_void;

    if (start) {
        UWORD anchor_x      = stack_frame[0];
        UWORD anchor_y      = stack_frame[1];
        UBYTE block_length  = (UBYTE)stack_frame[2];
        UBYTE max_angle     = (UBYTE)stack_frame[3];
        UBYTE speed_factor  = (UBYTE)stack_frame[4];
        UBYTE actor_idx     = (UBYTE)stack_frame[5];
        rope_trigger_enter(anchor_x, anchor_y, block_length, max_angle, speed_factor, actor_idx);
    }

    if (plat_state != ROPE_STATE) {
        return TRUE;
    }

    THIS->waitable = 1;
    return FALSE;
}

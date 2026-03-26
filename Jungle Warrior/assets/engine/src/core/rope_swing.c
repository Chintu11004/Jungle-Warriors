#pragma bank 255
#include "vm.h"
#include "states/platform.h"
#include "rope_swing.h"
#include "math.h"

BANKREF(ROPE_SWING)

/*
    rope_swing_update — called by VM_INVOKE every frame.

    stack_frame layout (PARAMS = .ARG6 = -7, so stack_frame[0] = first pushed):
        stack_frame[0] = anchor_x       (first pushed, deepest)
        stack_frame[1] = anchor_y
        stack_frame[2] = length
        stack_frame[3] = max_angle
        stack_frame[4] = speed_factor
        stack_frame[5] = start_side     (0 = -max_angle, 1 = +max_angle)
        stack_frame[6] = actor_idx

    On start == TRUE : initialise the rope state.
    Each frame       : yield (waitable = 1) and return FALSE while still swinging.
    Returns TRUE     : once the player has left ROPE_STATE — VM then pops the 7 stack slots.

    If same-rope re-grab cooldown is active (same rope actor id as last dismount), returns TRUE
    immediately (invoke completes, stack popped) so the script does not sit inside VM_INVOKE for
    many frames — cooldown still counts down in platform_update only, same PLAT_ROPE_SAME_GRAB_FRAMES.
    Retrigger next frame / next loop iteration is a no-op until cooldown clears. Other rope actors
    are unaffected.
*/
static UBYTE rope_swing_pending;

UBYTE rope_swing_update(void *THIS_void, UBYTE start, UWORD *stack_frame) OLDCALL BANKED {
    SCRIPT_CTX *THIS = (SCRIPT_CTX *)THIS_void;

    if (plat_state == ROPE_STATE) {
        rope_swing_pending = 0;
        THIS->waitable = 1;
        return FALSE;
    }

    if (start)
        rope_swing_pending = 1;

    if (rope_swing_pending) {
        UWORD anchor_x = stack_frame[0];
        UWORD anchor_y = stack_frame[1];
        UBYTE actor_idx = (UBYTE)stack_frame[6];
        if (plat_rope_same_grab_blocked(actor_idx)) {
            rope_swing_pending = 0;
            return TRUE;
        }
        {
            UBYTE block_length = (UBYTE)stack_frame[2];
            UBYTE max_angle = (UBYTE)stack_frame[3];
            UBYTE speed_factor = (UBYTE)stack_frame[4];
            UBYTE start_side = (UBYTE)stack_frame[5];
            rope_trigger_enter(anchor_x, anchor_y, block_length, max_angle, speed_factor, start_side, actor_idx);
        }
        rope_swing_pending = 0;
    }

    if (plat_state != ROPE_STATE) {
        return TRUE;
    }

    THIS->waitable = 1;
    return FALSE;
}

#pragma bank 255
#include "vm.h"
#include "states/platform.h"
#include "rope_swing.h"
#include "trigger.h"
#include "math.h"

BANKREF(ROPE_SWING)

/*
    rope_swing_update — called by VM_INVOKE every frame until done.

    stack_frame layout (set by eventRopeSwing.js):
        stack_frame[0] = frames   (pushed last  → ARG0, used as countdown)
        stack_frame[1] = blocks   (pushed first → ARG1, repurposed as subpx/frame)

    On start==TRUE  : initialise counter & per-frame displacement, snap to trigger edge.
    On start==FALSE : move player, decrement counter.
    Returns TRUE when counter reaches zero (VM pops the 2 stack slots).
*/
// UBYTE rope_swing_update(void * THIS_void, UBYTE start, UWORD * stack_frame) OLDCALL BANKED {
//     SCRIPT_CTX * THIS = (SCRIPT_CTX *)THIS_void;

//     if (start) {
//         UWORD frames = stack_frame[0];
//         UWORD blocks = stack_frame[1];

//         /* Snap player to right edge of the trigger that launched this script */
//         //trigger_t *trig = &triggers[last_trigger];
//         //PLAYER.pos.x = TILE_TO_SUBPX(trig->right);

//         /* Replace blocks slot with sub-pixel displacement per frame */
//         stack_frame[1] = (UWORD)(TILE_TO_SUBPX(blocks) / frames);
//     }

//     /* Move player one step */
//     PLAYER.pos.x += (INT16)stack_frame[1];

//     /* Release the auto-lock each frame so player input is still processed */
//     if (THIS->lock_count > 0) {
//         THIS->lock_count--;
//         vm_lock_state--;
//     }

//     stack_frame[0]--;

//     if (stack_frame[0] == 0) return TRUE;   /* done — VM pops the 2 stack slots */

//     THIS->waitable = 1;
//     return FALSE;   /* not done — VM_INVOKE rewinds PC for next frame */
// }*/

void rope_swing_enter(SCRIPT_CTX *THIS, INT16 anchor_x, INT16 anchor_y, INT16 length, INT16 max_angle) OLDCALL BANKED {
    // Trigger the rope state transition
    rope_trigger_enter(anchor_x, anchor_y, length, max_angle);
}
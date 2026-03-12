#pragma bank 255

#include "vm.h"
#include "deferred_actor.h"
#include "actor.h"
#include "data_manager.h"
#include "linked_list.h"
#include "macro.h"

BANKREF(DEFERRED_ACTOR)

static UBYTE actor_list_contains(actor_t *head, actor_t *actor) {
    UBYTE i;
    for (i = 0; head && i < MAX_ACTORS; i++) {
        if (head == actor) return TRUE;
        head = head->next;
    }
    return FALSE;
}

/*
 * Deferred Actor Load/Unload — VM_INVOKE handlers (one-shot).
 *
 * stack_frame layout (1 param):
 *   [0] actor index (from actorPushById)
 *
 * Current behaviour:
 *   - load: allocate or reuse a deferred VRAM slot, then register the actor
 *     on the inactive list without unhiding or activating it
 *   - unload: remove the actor from runtime lists, terminate active scripts,
 *     hide/disable it, and free the deferred slot
 *
 * The broader plan in DEFERRED_ACTOR_VRAM_PLAN.md is still not complete:
 * scene sprite lazy loading and emote tile reservation remain separate work.
 */
UBYTE deferred_actor_load(void *THIS_void, UBYTE start, UWORD *stack_frame) OLDCALL BANKED {
    (void)THIS_void;
    (void)start;
    UBYTE actor_idx = (UBYTE)stack_frame[0];
    if (actor_idx == 0 || actor_idx >= actors_len) return TRUE;
    actor_t * actor = actors + actor_idx;
    if (actor->reserve_tiles) return TRUE;  /* skip reserved actors */
    if (load_actor_sprite(actor) &&
        !actor_list_contains(actors_inactive_head, actor) &&
        !actor_list_contains(actors_active_head, actor)) {
        DL_PUSH_HEAD(actors_inactive_head, actor);
    }
    return TRUE;
}

UBYTE deferred_actor_unload(void *THIS_void, UBYTE start, UWORD *stack_frame) OLDCALL BANKED {
    (void)THIS_void;
    (void)start;
    UBYTE actor_idx = (UBYTE)stack_frame[0];
    if (actor_idx == 0 || actor_idx >= actors_len) return TRUE;
    actor_t * actor = actors + actor_idx;
    unload_actor_sprite(actor);
    return TRUE;
}

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
 *   - load: allocate or reuse a deferred VRAM slot, register the actor on the
 *     inactive list, and activate if on screen so the sprite shows immediately
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
    /* 0 = player; 1 = first NPC — never use deferred unload path; load still ok if needed */
    if (actor_idx == 0 || actor_idx >= actors_len) return TRUE;
    if (actor_idx == 1) return TRUE;  /* actors[1]: VRAM from load_scene, never deferred */
    actor_t * actor = actors + actor_idx;
    if (actor->reserve_tiles) return TRUE;  /* skip reserved actors */
    {
        UBYTE was_orphan = !actor_list_contains(actors_inactive_head, actor) &&
                           !actor_list_contains(actors_active_head, actor);
        if (load_actor_sprite(actor) && was_orphan) {
            activate_actor(actor);  /* activate if on screen so sprite shows immediately */
        }
    }
    return TRUE;
}

UBYTE deferred_actor_unload(void *THIS_void, UBYTE start, UWORD *stack_frame) OLDCALL BANKED {
    (void)THIS_void;
    (void)start;
    UBYTE actor_idx = (UBYTE)stack_frame[0];
    if (actor_idx == 0 || actor_idx == 1 || actor_idx >= actors_len) return TRUE;
    actor_t * actor = actors + actor_idx;
    unload_actor_sprite(actor);
    return TRUE;
}

/** One-shot VM_INVOKE handler for Deload Prev Level. Resets deferred slots and reclaims VRAM.
 *  Call after unloading all deferred actors for the current level, before loading the next level.
 *  No params. Returns TRUE immediately. */
UBYTE deload_prev_level_invoke(void *THIS_void, UBYTE start, UWORD *stack_frame) OLDCALL BANKED {
    (void)THIS_void;
    (void)start;
    (void)stack_frame;
    deload_prev_level();
    return TRUE;
}

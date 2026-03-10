#pragma bank 255

#include "vm.h"
#include "deferred_actor.h"
#include "actor.h"
#include "data_manager.h"
#include "linked_list.h"
#include "macro.h"

BANKREF(DEFERRED_ACTOR)

#define MAX_DEFERRED_ACTOR_SLOTS 10

/*
 * Deferred Actor Load/Unload — VM_INVOKE handlers (one-shot).
 *
 * stack_frame layout (1 param):
 *   [0] actor index (from actorPushById)
 *
 * These handlers are stubs. The full implementation requires the deferred
 * runtime from DEFERRED_ACTOR_VRAM_PLAN.md:
 *   - Deferred slot table (actor_idx, base_tile, tile_count per slot)
 *   - Per-actor VRAM state (static / deferred-unloaded / deferred-loaded)
 *   - load_scene changes for init_data=TRUE/FALSE
 *   - scene_sprite_ensure_loaded, emote_base_tile split, etc.
 *
 * Until then, these return TRUE immediately (no-op).
 */
UBYTE deferred_actor_load(void *THIS_void, UBYTE start, UWORD *stack_frame) OLDCALL BANKED {
    (void)THIS_void;
    (void)start;
    UBYTE actor_idx = (UBYTE)stack_frame[0];
    if (actor_idx == 0 || actor_idx >= actors_len) return TRUE;
    actor_t * actor = actors + actor_idx;
    if (actor->reserve_tiles) return TRUE;  /* skip reserved actors */
    load_actor_sprite(actor);
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

#pragma once
#include "vm.h"

BANKREF_EXTERN(DEFERRED_ACTOR)

/** One-shot VM_INVOKE handler for Deferred Actor Load event.
 *  stack_frame[0] = actor index (resolved from actor UUID)
 *  Returns TRUE immediately after processing (no blocking).
 */
UBYTE deferred_actor_load(void *THIS, UBYTE start, UWORD *stack_frame) OLDCALL BANKED;

/** One-shot VM_INVOKE handler for Deferred Actor Unload event.
 *  stack_frame[0] = actor index
 *  Returns TRUE immediately after processing (no blocking).
 */
UBYTE deferred_actor_unload(void *THIS, UBYTE start, UWORD *stack_frame) OLDCALL BANKED;

# Deferred Actor VRAM Implementation — Chat Transcript

This document summarizes the deferred actor loading/unloading implementation for the Jungle Warriors game engine. Use it to inform future AI sessions about the current state and design decisions.

---

## Overview

We implemented a **slot table** for deferred actor sprite loading—essentially a small virtual heap that allows VRAM reuse when actors are unloaded and reloaded. This differs from the standard `allocated_sprite_tiles` cursor, which only ever moves forward.

---

## Key Files

- **`assets/engine/src/core/data_manager.c`** — `load_actor_sprite`, `unload_actor_sprite`, slot table, `spritesheet_get_tile_count`
- **`assets/engine/src/core/deferred_actor.c`** — VM_INVOKE handlers for Deferred Actor Load/Unload events
- **`DEFERRED_ACTOR_VRAM_PLAN.md`** — Original design plan (not fully implemented)

---

## Slot Table Design

### Data Structures

```c
#define MAX_DEFERRED_ACTOR_SLOTS 10
#define DEFERRED_SLOT_FREE       0xFF

typedef struct {
    UBYTE actor_idx;    // 0xFF = free slot
    UBYTE base_tile;    // VRAM tile index
    UBYTE tile_count;   // Number of tiles in this slot
} deferred_slot_t;

static deferred_slot_t deferred_slots[MAX_DEFERRED_ACTOR_SLOTS];
```

### Invariants

- **actor_idx == 0xFF** → slot is free
- **base_tile == 0xFF** → slot never used (needs new allocation)
- **base_tile != 0xFF** and **actor_idx == 0xFF** → previously used, now freed (can reuse if tile_count >= n_tiles)
- At most 10 deferred actors loaded at once

### `load_actor_sprite` (deferred path, reserve_tiles == 0)

1. **Loop 1 — Already loaded?**  
   If any slot has `actor_idx == actor_idx`, return existing `base_tile` and `tile_count` (no-op).

2. **Loop 2 — Find free slot** (`actor_idx == 0xFF`):
   - **Never-used** (`base_tile == 0xFF`): Load at `allocated_sprite_tiles`, fill slot, advance cursor, **clear HIDDEN|DISABLED**.
   - **Previously used** (`base_tile != 0xFF` and `tile_count >= n_tiles`): Load at `base_tile` (reuse), set `actor_idx`, **clear HIDDEN|DISABLED**.
   - **Too small** (`tile_count < n_tiles`): Skip, try next slot.

### `unload_actor_sprite`

- Sets `actor_idx = DEFERRED_SLOT_FREE` (frees slot)
- **Does NOT** change `base_tile` or `tile_count` (needed for reuse)
- Removes actor from active/inactive lists
- Sets `HIDDEN | DISABLED`
- Terminates scripts if actor was active

### `deferred_slots_reset`

- Called from `load_scene(..., init_data=TRUE)` only
- `memset(deferred_slots, DEFERRED_SLOT_FREE, ...)` — all slots free, base_tile/tile_count = 0xFF

---

## Helper: `spritesheet_get_tile_count`

Reads tile count from spritesheet metadata **without loading into VRAM**. Returns max of DMG and CGB tile counts so one slot works on both targets. Used for first-fit reuse check (`tile_count >= n_tiles`).

---

## Differences from Standard `allocated_sprite_tiles`

| Aspect | Standard | Deferred Slot Table |
|--------|----------|---------------------|
| Cursor | Always moves forward | Moves only on new allocations |
| Unload effect | None | Marks slot free for reuse |
| VRAM reuse | No | Yes (first-fit over freed slots) |
| Shared sprites | Yes (`scene_sprites_base_tiles`) | No (per-actor private slots) |

---

## Important Fixes Applied

1. **Never reuse never-used slots in first-fit** — Slots with `base_tile == 0xFF` must go through "allocate new" path (load at `allocated_sprite_tiles`), not first-fit. Otherwise we'd load at tile 255 and overflow.

2. **Clear HIDDEN|DISABLED on load** — `unload_actor_sprite` sets these flags and removes the actor from lists. `load_actor_sprite` must clear them so the actor renders again.

3. **Actor list handling** — At scene load, all actors (including deferred) are added to `actors_inactive_head`. After unload, they're removed. The current implementation does NOT re-add to the list in `load_actor_sprite`; actors stay in the list from `load_scene` until unloaded. If an actor was unloaded and then reloaded, they would NOT be in any list—this may need fixing (add to inactive if not in either list).

---

## Known Issues / Not Yet Implemented

1. **VRAM overlap / background corruption** — Sprite allocations can overlap with background tile indices. First-fit reuse may load into `base_tile` values that overlap the background. Consider restricting reuse to slots with `base_tile >= 128` (sprite-only region) or ensuring allocation order keeps sprites in a safe range.

2. **Re-add to list on load** — If an actor was unloaded (removed from lists) and then reloaded, they need to be pushed back to `actors_inactive_head`. The earlier implementation had `DL_PUSH_HEAD(actors_inactive_head, actor)` with a check to avoid duplicates. This was removed during the slot-table refactor.

3. **Projectiles** — Per the plan, projectiles should use `scene_sprite_ensure_loaded` (not implemented). Currently they read `scene_sprites_base_tiles` which is UNLOADED at init.

4. **Emote base tile split** — Plan says to reserve fixed `emote_base_tile`; currently emotes share `allocated_sprite_tiles` with deferred actors.

5. **Scene reload (init_data=FALSE)** — Deferred-loaded actors are not reloaded into their slots on scene reload; only reserved actors are.

---

## `actors` Array

- `actors[MAX_ACTORS]` — Global array of all actors in the scene
- `actors[0]` = PLAYER
- Active/inactive state is tracked via `actors_active_head` and `actors_inactive_head` (linked lists) and flags (HIDDEN, DISABLED, ACTIVE)
- The array itself is static; actors are never created/destroyed, only moved between lists and toggled flags

---

## Rendering

- `actors_render` iterates from `PLAYER.prev` through the active list
- Skips actors with `ACTOR_FLAG_HIDDEN | ACTOR_FLAG_DISABLED`
- Uses `actor->base_tile` and `actor->frame` with `move_metasprite_clipped`
- No `ReadBankedFarPtr` needed at render time; `base_tile` is already set by `load_actor_sprite`

---

## Event Flow

1. **Deferred Actor Load** event → `deferred_actor_load` → `load_actor_sprite(actor)` → loads tiles, sets base_tile, clears HIDDEN|DISABLED
2. **Deferred Actor Unload** event → `deferred_actor_unload` → `unload_actor_sprite(actor)` → frees slot, removes from lists, sets HIDDEN|DISABLED

Scripts may need to explicitly activate the actor after load if they want it to update (e.g. `VM_ACTOR_ACTIVATE`).

# Deferred Actor VRAM Loading Plan

## Summary

- Change actor sprite handling from scene-wide shared preload to deferred per-actor private allocation.
- On `load_scene(..., init_data=TRUE)`, load only the player, actors with `reserve_tiles != 0`, and scene sprites needed by projectiles. Do not preload non-reserved actor sprite tiles.
- Keep all actor metadata in `actors[]`; do not add a second metadata store. Add only runtime VRAM/state tables.
- Add explicit custom events for batch load and batch unload of deferred actors, following the existing rope-swing plugin pattern.

## Implementation Changes

- Add a deferred runtime layer with external side tables, not new actor flags, because all `actor->flags` bits are already used.
- Introduce `MAX_DEFERRED_ACTOR_SLOTS = 10` and track per-slot `actor_idx`, `base_tile`, and `tile_count`.
- Track per-actor VRAM state in a small runtime table with three states: static/preloaded, deferred-unloaded, deferred-loaded.
- Add a helper to read spritesheet tile count without writing VRAM. Reserve the max of DMG and CGB tile counts so one slot works on both targets.
- Split emote storage from the allocation cursor:
  - Reserve a fixed `emote_base_tile` after static scene allocations.
  - Stop using mutable `allocated_sprite_tiles` as both emote base and deferred allocation cursor.
  - Start deferred allocations after `emote_base_tile + EMOTE_SPRITE_SIZE`.
- Use a first-fit allocator over the deferred slot table. Explicit unload marks a slot free; there is no compaction. Batch load is best-effort in input order.

## Scene Load and Runtime Rules

- In `load_scene(..., init_data=TRUE)`:
  - Reset the scene sprite cache and deferred slot tables.
  - Load player sprite exactly as today.
  - Reintroduce scene sprite loading only through a helper like `scene_sprite_ensure_loaded(...)`; do not restore the old preload-all loop.
  - Copy all actors into `actors[]`.
  - For `reserve_tiles != 0`, keep current behavior: assign `base_tile`, load tiles immediately, and reserve private space.
  - For `reserve_tiles == 0`, load animations and bounds, set `HIDDEN | DISABLED`, set VRAM state to deferred-unloaded, clear any valid `base_tile`, and remove the actor from both active and inactive lists so generic activation does not touch it.
- In `load_scene(..., init_data=FALSE)`:
  - Preserve deferred actor state and slot ownership.
  - Reload all reserved actors into their fixed tiles as today.
  - Reload every deferred-loaded actor into its stored slot.
  - Rebuild projectile `base_tile` values through `scene_sprite_ensure_loaded(...)` so projectile behavior survives scene reloads.
- Rework projectile sprite resolution:
  - During scene projectile setup and in `vm_projectile_load_type`, resolve the scene sprite index and call `scene_sprite_ensure_loaded(...)`.
  - `scene_sprites_base_tiles[]` becomes a lazy cache for projectile/shared non-actor use, not an actor preload table.

## Event and VM Surface

- Add two custom events:
  - `Deferred Actor Load Batch`
  - `Deferred Actor Unload Batch`
- Event shape for both:
  - `actor_count`
  - `actor_1` through `actor_10`
- Compile path:
  - Follow the rope-swing pattern with a plugin JS file plus a macro/helper entry in `vm.i`.
  - Use the existing invoke/call infrastructure to reach banked C helpers; do not add a new bytecode opcode.
- Load batch behavior:
  - Process the first `actor_count` actor IDs in order.
  - Skip player, reserved/static actors, invalid indices, duplicates within the same batch, and actors already deferred-loaded.
  - Allocate a private slot, load the sprite tiles, store `base_tile`, mark deferred-loaded, and insert the actor into the inactive list.
  - Do not activate or unhide the actor; scripts must explicitly show/activate afterward.
- Unload batch behavior:
  - Process only deferred-loaded actors.
  - If active, terminate update/hit scripts and remove from the active list.
  - If inactive, remove from the inactive list.
  - Set `HIDDEN | DISABLED`, clear `base_tile`, mark deferred-unloaded, and free the slot.
- Repeat load behavior is a no-op for already loaded actors.

## Test Plan

- Fresh scene load:
  - Verify only player, reserved actors, emote reservation, and projectile-needed scene sprites consume VRAM.
  - Verify non-reserved actors exist in `actors[]` but are absent from both actor lists and never render.
- Load batch:
  - Load 1 actor, then several actors, then the same actor again.
  - Confirm slot assignment, tile load, inactive-list insertion, and no-op on repeated load.
- Activation path:
  - After load batch, explicitly clear flags and activate via existing script flow; confirm the actor renders and updates normally.
- Unload batch:
  - Unload inactive and active deferred actors.
  - Confirm scripts terminate, the actor is removed from all lists, the slot becomes reusable, and later reload succeeds.
- Scene reload:
  - Call `load_scene(current_scene, ..., FALSE)` after deferred actors were loaded.
  - Confirm reserved actors, deferred-loaded actors, projectile sprites, and emotes all reappear with correct tiles.
- Capacity/failure:
  - Request more than 10 concurrent deferred actors or exceed remaining sprite VRAM.
  - Confirm best-effort load order, no corruption of already loaded actors, and all failed actors remain deferred-unloaded.

## Assumptions

- Deferred loading applies only to non-player actors with `reserve_tiles == 0`.
- Maximum concurrent deferred actor sprite allocations is 10.
- Event plugins use fixed actor slots because this repo does not show support for true array fields.
- Load is intentionally separate from activate/show.
- No second actor metadata array is needed; runtime tables only track VRAM ownership/state.


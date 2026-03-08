# Summary of Rope-Related Changes in `actor.c`

This document summarizes the modifications made to `assets/engine/src/core/actor.c` to support large rope metasprites (~128px) and seamless rope loading when scrolling.

---

## 1. Constants (lines 34–35)

- **`ROPE_MARGIN_TILE16`** (4) – Extra blocks around the viewport where rope actors stay enabled. They don’t deactivate until outside this margin.
- **`ROPE_ACTIVATION_TILES`** (8) – Tiles the rope extends from its origin (for early activation when scrolling).

---

## 2. `move_metasprite_clipped` (lines 72–97)

New **NONBANKED** function that replaces GBDK’s `move_metasprite` for all actors (except emote and player):

- Uses **INT16** for x/y to avoid uint8 wrapping and ghost sprites.
- Matches GBDK `__move_metasprite`: dy/dx are cumulative deltas, no extra +16/+8.
- For each sub-sprite: if `(UWORD)y >= 256` or `(UWORD)x >= 256`, sets `y = 0` to hide instead of writing garbage.
- Writes to `__render_shadow_OAM` for double buffering.
- Prevents kernel panics and visual glitches with large metasprites (~128px).

---

## 3. `actors_render` (lines 266–294)

- All actors now use `move_metasprite_clipped` instead of `move_metasprite` when rendering.
- Player and emote still use `move_metasprite`.

---

## 4. `actors_update` (lines 177–188)

- For rope actors (`actors[1]` and `actors[2]`): instead of immediately deactivating when off-screen, use a **4-block margin** (`ROPE_MARGIN_TILE16`). If the anchor is within this margin, the rope stays active; it’s only deactivated when fully outside.
- Ropes are never passed to `deactivate_actor_impl` in this path.

---

## 5. `activate_actor_impl` (lines 363–369)

- Normally, off-screen actors return early and are not activated.
- For rope actors: don’t return early; allow activation even when slightly off-screen so they can be activated when only part of the rope is on-screen.

---

## 6. `activate_actors_in_row` (lines 388–402)

When loading a horizontal strip (row `y`, columns `[x, x_end)`):

- **Normal actors**: activate if anchor tile equals row `y` and column is in `[x, x_end)`.
- **Rope actors**: activate if:
  - `ty <= y <= ty + ROPE_ACTIVATION_TILES` (rope overlaps row `y`; anchor at or above, bottom reaches row).
  - Strip `[x, x_end)` overlaps `[tx - 8, tx + 8]` (rope’s horizontal swing range).

This makes ropes activate when their bottom enters newly loaded rows before you reach the anchor.

---

## 7. `activate_actors_in_col` (lines 405–419)

When loading a vertical strip (column `x`, rows `[y, y_end)`):

- **Normal actors**: activate if anchor column equals `x` and row is in `[y, y_end)`.
- **Rope actors**: activate if:
  - Column `x` is within 8 tiles of anchor `tx`.
  - `[ty, ty + ROPE_ACTIVATION_TILES]` overlaps `[y, y_end)` (i.e. `ty < y_end && (ty + 8) > y`).

This makes ropes activate when their bottom enters newly loaded columns before you reach the anchor.

---

## Actors Affected

All rope-specific logic applies to **`actors[1]`** and **`actors[2]`** (indices 1 and 2).

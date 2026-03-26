# Rope swing (`ROPE_STATE`) — `platform.c`

This document describes how the platformer rope swing state works in `assets/engine/src/states/platform.c`, how it connects to the VM/script layer, and a **planned approach for reverse / mirrored swing** (not implemented in C yet; `start_side` is still passed through `rope_trigger_enter` for you to wire up).

For rope **actor rendering** (large metasprites, scroll activation), see [`ACTOR_ROPE_CHANGES.md`](../ACTOR_ROPE_CHANGES.md) (`actor.c`).

---

## Entry from scripts

- **`rope_trigger_enter`** (`platform.h`) stores anchor, length, max angle, swing speed, rope actor index, and a reserved **`start_side`** argument (currently unused in `state_enter_rope`; cast to void to silence warnings).
- **`rope_swing_update`** (`assets/engine/src/core/rope_swing.c`) is invoked by **`VM_ROPE_SWING`** (`vm.i`) / the **Rope Swing** plugin (`plugins/rope_swing/events/eventRopeSwing.js`). It calls `rope_trigger_enter` once, then yields every frame until `plat_state != ROPE_STATE`.

---

## State machine

- **`state_enter_rope`** — Runs when entering `ROPE_STATE`: sets initial `plat_rope_theta`, seeds `plat_rope_ang_vel` from approach velocity, rope actor animation, input mask, `ROPE_INIT` callback.
- **`state_update_rope`** — Each frame: optional jump → tangential launch + push to `plat_rope_resetPositions_array` for the rope actor “coast” animation; else integrate pendulum-like motion and update player position + rope sprite frame.
- **`state_exit_rope`** — Clears mask, `ROPE_END` callback.

---

## Angle representation

- **`plat_rope_theta`** — `WORD` fixed-point angle: **4 sub-angle bits** (`<< 4` when initializing from index units). Integer “angle index” is **`plat_rope_theta >> 4`** (same units as `plat_rope_max_angle`).
- **`plat_rope_max_angle`** — Not degrees in the engine. Set in **`rope_trigger_enter`** from degrees (30–90) as:

  `plat_rope_max_angle = (max_angle_degrees * 64) / 90`

  so the index is in the same space as the 256-entry `SIN`/`COS` tables in `sincos.h`.

- **Table index for trig** — `currAngleIdx = (plat_rope_theta >> 4)`; if negative, **`256 + currAngleIdx`** so lookups stay in `0…255`.

---

## Player position (arc)

Each frame (when not jumping):

- `dx = L * SIN(currAngleIdx) >> 7`
- `dy = L * COS(currAngleIdx) >> 7`  
  with `L = TILE_TO_SUBPX(plat_rope_block_length)`.

Player is placed at `anchor + (dx, dy)`.

---

## Angular motion (tuning)

- **`ang_accel = -SIN(currAngleIdx) / (plat_rope_block_length * plat_rope_swing_speed)`**
- **`plat_rope_ang_vel += ang_accel`**
- **`plat_rope_theta += plat_rope_ang_vel`**

Higher **`plat_rope_swing_speed`** (1–5 from the event) **increases** the divisor → **slower** swing.

---

## Rope sprite frames

`signed_idx` is clamped to `±plat_rope_max_angle`, then:

`rope_frame = (signed_idx + max) * 28 / (2 * max)` → **0…28** relative to `frame_start`.

---

## Jump release

Uses current `plat_rope_theta` / `plat_rope_ang_vel` with wrapped `curr_angle_idx` and **`RELEASE_AND_INIT_FACTOR`** (9) to set `plat_vel_x` / `plat_vel_y`. If `plat_vel_x < 0`, `PLAYER.dir = DIR_LEFT`.

---

## Post-exit rope animation

`reset_rope_actor_position` advances `rope_actor_t` entries (theta, ang_vel, max angle, `rope_block_length * swing_speed`, actor index) until `rope_frame == 0`, then removes the entry.

---

## Reverse / mirrored swing (plan — for you to implement)

The default behavior always **starts at the negative extreme**: `plat_rope_theta = -plat_rope_max_angle << 4` and seeds ω using index **`256 - plat_rope_max_angle`**.

To support a **mirror swing** (start at **+max** and/or play rope frames **backward**):

1. **`state_enter_rope`**  
   - If `start_side != 0`: set `plat_rope_theta = plat_rope_max_angle << 4` (positive extreme).  
   - Seed ω using **`plat_rope_max_angle`** as the trig index (same `v_tan = vx*COS(idx) - vy*SIN(idx)` pattern, but with the index for **+θ**).

2. **Position vs torque (optional split)**  
   - Wrapped **256+idx** `SIN`/`COS` for **player position** is asymmetric vs true **sin(θ)** for **|θ|**; if you need a symmetric **arc** for **+θ** while keeping **tuning** for speed, you may compute **dx/dy** with **symmetric** `sin(θ)` / `cos(|θ|)` while keeping **`ang_accel`** on the existing wrapped **`SIN(currAngleIdx)`** formula (or vice versa — tune and test).

3. **Rope sprite frames**  
   - If art is authored for left→right only: **`rope_frame = 28 - rope_frame`** when `start_side != 0`, or flip the actor’s facing if your renderer supports it.

4. **`reset_rope_actor_position`**  
   - If you mirror frames, store a flag in **`rope_actor_t`** and mirror there too; **pop** when the **logical** base frame hits the end condition (e.g. compare **before** mirroring, so “done” does not depend on the mirrored tile index).

5. **`VM_ROPE_SWING` / plugin** — Already pass **`START_SIDE`** on the stack; **`rope_trigger_enter`** should assign a **`plat_rope_start_side`** (or similar) once you wire **`state_enter_rope`** and frame logic.

---

## Files to touch for reverse swing

| Area | File |
|------|------|
| Enter / update / reset | `assets/engine/src/states/platform.c` |
| `rope_actor_t` + `rope_trigger_enter` | `assets/engine/include/states/platform.h` |
| VM stack + invoke count | `assets/engine/include/vm.i` |
| `rope_swing_update` stack | `assets/engine/src/core/rope_swing.c` |
| GB Studio event | `plugins/rope_swing/events/eventRopeSwing.js` |

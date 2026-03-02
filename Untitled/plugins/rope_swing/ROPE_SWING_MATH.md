# Rope Swing — Pendulum Math Reference

This document explains every piece of math behind the rope swing state in
`platform.c`, from the sine table all the way to the release-velocity formula.
Use it as a self-contained reference when rewriting the implementation.

---

## 1. The Sine Table

The engine stores a 256-entry lookup table (`sine_wave[256]`).  
One full circle = 256 entries, so each entry covers:

$$
\frac{360°}{256} = 1.40625° \text{ per table index}
$$

Every value is an `int8_t` in the range **[−127, 127]**, where 127 ≈ 1.0 and
−127 ≈ −1.0.  To convert a table value to a true fraction, divide by **128**
(use `>> 7` in C):

$$
\sin_{\text{true}}(\theta) = \frac{\text{SIN}(\theta_{\text{idx}})}{128}
$$

`COS` is defined as a 90-index phase shift:

```c
COS(n) = SIN((uint8_t)(n + 64))
```

**Key index values:**

| Index (uint8) | Angle   | SIN value |
|---------------|---------|-----------|
| 0             | 0°      | 0         |
| 32            | 45°     | 83        |
| 64            | 90°     | 127       |
| 128           | 180°    | 0         |
| 192           | 270°    | −127      |
| 224           | 315°    | −90       |

Because the index is `uint8_t`, negative angles wrap naturally:

$$
\text{(uint8\_t)}(-32) = 224 \quad \Rightarrow \quad \text{SIN}(224) = -90 \approx -\sin(45°)
$$

---

## 2. Angle Representation

All angles in the implementation are in **sine-table-index units**.

Converting from degrees:

$$
\theta_{\text{idx}} = \left\lfloor \frac{\text{degrees} \times 256}{360} \right\rfloor
= \left\lfloor \frac{\text{degrees} \times 64}{90} \right\rfloor
$$

Examples:

| Degrees | Table index |
|---------|-------------|
| 30°     | 21          |
| 45°     | 32          |
| 60°     | 43          |
| 90°     | 64          |

The pendulum swings between **−max\_angle\_idx** (far left) and
**+max\_angle\_idx** (far right).  Positive θ = right of anchor, negative θ =
left of anchor.

---

## 3. Player Position from Angle

The rope is a rigid pendulum with its pivot at `(anchor_x, anchor_y)` and
length `L` tiles.  Standard polar-to-Cartesian for a pendulum hanging
downward:

$$
\text{player\_x} = \text{anchor\_x} + L \cdot \sin(\theta)
$$

$$
\text{player\_y} = \text{anchor\_y} + L \cdot \cos(\theta)
$$

`cos` is used for Y because GB's Y axis points **downward**: when θ = 0
(rope straight down), `cos(0) = 1` and the player is directly below the
anchor.

**In subpixels** (1 tile = 256 subpixels, SIN normalised by 128):

$$
dx = \frac{L_{\text{subpx}} \times \text{SIN}(\theta_{\text{idx}})}{128}
$$

$$
dy = \frac{L_{\text{subpx}} \times \text{COS}(\theta_{\text{idx}})}{128}
$$

$$
\text{player\_pos\_x} = \text{anchor\_subpx\_x} + dx
$$
$$
\text{player\_pos\_y} = \text{anchor\_subpx\_y} + dy
$$

This is an **absolute position** recalculated every frame — not a delta.
The per-frame displacement is the natural result of θ changing each frame.

---

## 4. The Pendulum Equation

The real equation of motion for a simple pendulum is:

$$
\ddot{\theta} = -\frac{g}{L} \sin(\theta)
$$

Discretised to one frame (Euler integration):

$$
\alpha = -\frac{\sin(\theta)}{L} \quad \text{(angular acceleration)}
$$

$$
\omega \mathrel{+}= \alpha \quad \text{(angular velocity accumulates)}
$$

$$
\theta \mathrel{+}= \omega \quad \text{(angle updates)}
$$

Here `g` is absorbed into a tuning constant K (default **K = 1**, can be
increased to make swings faster).  The division by `L` is the key physical
relationship:

$$
\omega_{\text{natural}} \propto \sqrt{\frac{g}{L}}
\quad \Rightarrow \quad
\text{shorter rope} = \text{larger } \alpha = \text{faster swing}
$$

No separate "speed" hack is needed — the rope length does the right thing
automatically.

### L in subpixels vs tiles

When `L` is stored in **subpixels** (1 tile = 256 subpixels), dividing
directly by `L_subpx` would make α tiny (e.g. −83 / 1792 = 0 in integer
math).  Scale the numerator by **256** to compensate:

$$
\alpha = -\frac{\text{SIN}(\theta_{\text{idx}}) \times 256}{L_{\text{subpx}}}
$$

This is mathematically identical to the tile version because
$L_{\text{subpx}} = L_{\text{tiles}} \times 256$, so the 256s cancel:

$$
\frac{\text{SIN} \times 256}{L_{\text{tiles}} \times 256} = \frac{\text{SIN}}{L_{\text{tiles}}}
$$

The intermediate product `SIN × 256` reaches at most `127 × 256 = 32512`,
which fits in `INT16` (max 32767) — but only just.  Use `int32_t` for the
multiply to be safe.

---

## 5. Why a Precision Scale Is Needed (ROPE\_SCALE)

### The problem with plain integers

Suppose `L = 1280` subpx (5 tiles) and `θ_idx = 32` (45° right):

$$
\alpha = -\frac{\text{SIN}(32) \times 256}{1280} = -\frac{83 \times 256}{1280} = -\frac{21248}{1280} = -16.6
$$

C integer division truncates toward zero:

$$
-21248 \div 1280 \;\longrightarrow\; -16 \qquad (0.6 \text{ discarded every frame})
$$

The pendulum bleeds 0.6 table-units of angular acceleration every frame.
Over a full swing this accumulates into significant energy loss, and the
swing slows down and eventually stops.

### The fix: store both θ and ω at ROPE\_SCALE times their true value

Define `ROPE_SCALE = 16`.  Now:

$$
\theta_{\text{stored}} = \theta_{\text{true}} \times 16
\qquad
\omega_{\text{stored}} = \omega_{\text{true}} \times 16
$$

The acceleration formula produces values that are **already** in stored
units, because the SIN output and the division together land in the right
scale:

$$
\alpha_{\text{stored}} = -\frac{\text{SIN}(\theta_{\text{stored}} / 16) \times 256}{L_{\text{subpx}}}
= -\frac{83 \times 256}{1280} = -16 \quad \text{(stored units)}
$$

This still truncates to −16, but now −16 means **−1.0 in true units**, not −1.6.
The fraction is preserved frame-over-frame because `ω_stored` keeps
accumulating integer steps:

| Frame | α\_stored | ω\_stored | θ\_stored | θ\_true |
|-------|-----------|-----------|-----------|---------|
| 0     | —         | 0         | −512      | −32.0   |
| 1     | −16       | −16       | −528      | −33.0   |
| 2     | −15       | −31       | −559      | −34.9   |
| 3     | −14       | −45       | −604      | −37.8   |
| …     | …         | …         | …         | …       |

The sub-unit velocity builds up properly across frames.

### Getting the raw table index back

$$
\theta_{\text{idx}} = \lfloor \theta_{\text{stored}} / \text{ROPE\_SCALE} \rfloor
$$

In C: `theta_idx = stored_theta / ROPE_SCALE`

### Initialisation at −max\_angle

$$
\theta_{\text{stored, initial}} = -\theta_{\text{max\_idx}} \times \text{ROPE\_SCALE}
$$

For 45° max swing: $-32 \times 16 = -512$

$$
\omega_{\text{stored, initial}} = 0
$$

---

## 6. Full Per-Frame Update (pseudocode)

```
// L is stored in subpixels (e.g. 7 tiles → 1792 subpx)

// --- Physics ---
theta_idx      =  stored_theta / ROPE_SCALE
alpha          = -(int32_t)SIN(uint8(theta_idx)) * 256 / L_subpx
stored_omega  +=  alpha
stored_theta  +=  stored_omega

// --- Position ---
theta_idx      =  stored_theta / ROPE_SCALE    // recalculate after update
sin_theta      =  SIN(uint8(theta_idx))
cos_theta      =  COS(uint8(theta_idx))
dx = ((int32_t)L_subpx × sin_theta) / 128
dy = ((int32_t)L_subpx × cos_theta) / 128

player_x = anchor_x_subpx + dx
player_y = anchor_y_subpx + dy
```

Note: storing L in subpixels removes the `rope_len_tiles × 256` intermediate
step from the position block — `L_subpx` is used directly.  The alpha formula
gains a `× 256` in the numerator to compensate; the net result is identical
to the tile-based version (see §4).

> **Important:** `theta_idx` is computed **twice** — once before updating
> `stored_theta` (to evaluate the restoring force at the current angle) and
> once after (to place the player at the new angle).  Using the pre-update
> index for position would lag the visual by one frame.

---

## 7. Release Velocity (Jump Off the Rope)

When the player releases, they fly off **tangentially** to the arc.  The
tangent direction is perpendicular to the radial (rope) direction:

$$
\text{radial:} \quad (\sin\theta,\; \cos\theta)
$$

$$
\text{tangent:} \quad (\cos\theta,\; -\sin\theta)
$$

The speed at release equals rope-length × angular-velocity:

$$
v_{\text{tangential}} = L \cdot \omega_{\text{true}}
\quad \text{(in subpixels/frame)}
$$

In component form:

$$
v_x = L_{\text{subpx}} \cdot \frac{\cos\theta}{128} \cdot \frac{\omega_{\text{stored}}}{\text{ROPE\_SCALE}}
$$

$$
v_y = -L_{\text{subpx}} \cdot \frac{\sin\theta}{128} \cdot \frac{\omega_{\text{stored}}}{\text{ROPE\_SCALE}}
$$

The `plat_vel` unit in the engine is `subpixels/frame × 128`.  The two
factors of 128 cancel (one from the SIN normalisation, one from the velocity
unit conversion), leaving:

$$
\texttt{plat\_vel\_x} = \frac{L_{\text{subpx}} \times \cos\theta \times \omega_{\text{stored}}}{\text{ROPE\_SCALE}}
$$

$$
\texttt{plat\_vel\_y} = \frac{L_{\text{subpx}} \times (-\sin\theta) \times \omega_{\text{stored}}}{\text{ROPE\_SCALE}}
$$

Clamp both results to `[WORD_MIN, WORD_MAX]` to prevent overflow.

---

## 8. Numeric Example

**Input:** anchor at tile (19, 3), rope 7 tiles → **1792 subpx**, 45° max swing.

```
anchor_x_subpx = 19 × 256 + 128 = 4992   ← +128 = mid-tile X
anchor_y_subpx =  3 × 256       =  768
L_subpx        =  7 × 256       = 1792    ← stored directly, no conversion needed
max_angle_idx  = (45 × 64) / 90 =   32

stored_theta   = −32 × 16 = −512
stored_omega   = 0
```

**Frame 1:**

```
theta_idx  = −512 / 16 = −32  →  (uint8)−32 = 224
SIN(224)   = −90
COS(224)   = SIN(288) = +83
alpha      = −(−90 × 256) / 1792 = 23040 / 1792 = +12   ← same result as −(−90)/7

stored_omega = 0 + 12 = 12
stored_theta = −512 + 12 = −500

theta_idx  = −500 / 16 = −32   (still rounds to −32)
dx = (1792 × −90) / 128 = −1260 subpx   ≈ −4.9 tiles  (left of anchor)
dy = (1792 ×  83) / 128 = +1162 subpx   ≈ +4.5 tiles  (below anchor)

player_x = 4992 − 1260 = 3732 subpx  = tile ~14.6
player_y =  768 + 1162 = 1930 subpx  = tile ~7.5
```

The player appears about 4.9 tiles left and 4.5 tiles below the anchor.
This is correct geometry for a 7-tile rope at −45°
(7 × sin 45° ≈ 4.95, 7 × cos 45° ≈ 4.95).

---

## 9. What Breaks Without the Scale

| Without ROPE\_SCALE | With ROPE\_SCALE = 16 |
|---|---|
| α = −83×256/1792 = **−11** (truncated) | α = −11 (stored units = −0.69 true) |
| θ steps by **11 table units/frame** = 15.5° | θ steps by **11/16 ≈ 1°/frame** |
| Player jumps ≈ 1 tile per frame | Player moves ≈ 0.07 tiles per frame |
| Looks like "teleporting blocks" | Smooth sub-pixel arc |

---

## 10. Variable Summary

| Variable | Type | Scale | Meaning |
|---|---|---|---|
| `plat_rope_anchor_x` | `UWORD` | subpixels | Rope pivot X |
| `plat_rope_anchor_y` | `UWORD` | subpixels | Rope pivot Y |
| `plat_rope_block_length` | `UWORD` | subpixels | Rope length L (set as `tiles × 256` on enter) |
| `plat_rope_max_angle` | `UBYTE` | table index | Max swing angle |
| `plat_rope_theta` | `INT16` | × ROPE\_SCALE | Current angle |
| `plat_rope_ang_vel` | `INT16` | × ROPE\_SCALE | Current angular velocity |
| `ROPE_SCALE` | `#define` | — | Precision multiplier (16) |

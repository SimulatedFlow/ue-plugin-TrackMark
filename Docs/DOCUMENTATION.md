# TrackMark — Documentation

Unreal Engine 5.8 · Runtime module only · Win64 built and verified, Mac/Linux allow-listed but not built

---

## Contents

1. [The idea](#1-the-idea)
2. [Installation and first marks](#2-installation-and-first-marks)
3. [Triggering marks](#3-triggering-marks)
4. [Profiles](#4-profiles)
5. [Surfaces and physical materials](#5-surfaces-and-physical-materials)
6. [The budget and recycling](#6-the-budget-and-recycling)
7. [The master material](#7-the-master-material)
8. [The stats overlay](#8-the-stats-overlay)
9. [Blueprint reference](#9-blueprint-reference)
10. [Project settings](#10-project-settings)
11. [Console commands and variables](#11-console-commands-and-variables)
12. [Performance notes](#12-performance-notes)
13. [Limits and known caveats](#13-limits-and-known-caveats)
14. [Troubleshooting](#14-troubleshooting)

---

## 1. The idea

A footprint system has exactly one hard problem: there are a lot of footprints.

The obvious implementation spawns a `UDecalComponent` per print. That is one draw call per print, plus
one component, plus one actor to own it, plus a timer to destroy it. It looks fine in a test level with
one character and falls over at a few hundred marks — which is about four seconds of a squad walking.

TrackMark does the same thing the engine does for foliage. Every mark of one profile is an **instance**
in a single `UInstancedStaticMeshComponent`: one component, one draw call, no matter whether ten marks
or ten thousand are on the ground.

That leaves two costs, and both are bounded:

- **Placement.** One downward line trace plus two small writes. Counted in the stats as
  `Traces this frame` and `Placement ms`.
- **Ageing.** The material fades a mark out by itself; the CPU only has to notice when a mark's time is
  up so its slot can be reused. That sweep is capped per tick.

There is no per-mark tick, no per-mark actor, no per-mark component, and after warm-up no per-mark
allocation.

---

## 2. Installation and first marks

1. Copy the plugin into your project's `Plugins/` folder (or install it from Fab) and enable it.
2. Open a character Blueprint, **Add Component → Track Mark**.
3. Play.

You will see boot prints. Nothing else is required: the component falls back to a **code profile** that
exists in C++, and the mesh falls back to the engine's own `/Engine/BasicShapes/Plane`.

If you want the prints to appear under the actual feet rather than under the actor's centre, attach the
component to a foot socket, or add two components and drive them from animation notifies (§3).

### What ships in the plugin's content

`Content/TrackMark/` contains the master material `M_TrackMark` and the demo map. If the plugin's
content is not installed, or the material is missing, the system still runs: batches fall back to the
mesh's own material and one warning is logged. The geometry and the placement are correct; the marks
just do not read the custom data and therefore do not fade.

---

## 3. Triggering marks

Three triggers, and they combine freely.

### Distance (default)

`UTrackMarkComponent::TriggerMode = Distance`. The component ticks, measures **ground** distance since
the last mark, and drops one every stride length. This is the trigger that needs no animation work at
all, and it is what the four built-in profiles are tuned for.

- `StrideLengthOverride` — centimetres between marks. 0 uses the profile's `StrideLength`.
- `MinSpeed` — below this ground speed nothing is placed, so a character shuffling in place does not
  stamp the same spot forever.
- `bAlternateFeet` — alternates left and right, offset by the profile's `LateralSpacing`.

### Animation notify (accurate)

Set `TriggerMode = Manual / Anim Notify` and place the **Track Mark** notify (`UAnimNotify_TrackMark`)
on the foot-plant frames.

- `SocketName` — the foot bone. When set, the mark is placed centred on the socket, because the socket
  already carries the real lateral offset and the profile's spacing would double it up.
- `Side` — used when no socket is set; drives the lateral offset and the mirrored shape.
- `ComponentName` — which track mark component to drive when the actor has more than one.
- `OpacityScale` — a landing notify can stamp harder than a walk step.

In Manual mode the component's tick function is switched off entirely. A thousand idle components cost
nothing.

### Explicit calls

`LeaveTrack(Side)`, `LeaveTrackAt(Location, Forward, Side)` and `LeaveTrackAlternating()` on the
component, or `UTrackMarkStatics::LeaveTrackMark*` from anywhere with a world context.

Every trigger returns an `ETrackMarkResult`, so a caller can tell "placed" from "no ground", "too steep",
"disabled" or "no profile". Nothing fails silently.

---

## 4. Profiles

A `UTrackMarkProfile` holds the look **and** the behaviour of one kind of mark, and it is also the
**batching key**: one profile in use is one instanced component is one draw call. Two profiles that
share a mesh are still two batches, because each profile gets its own material instance carrying its
decay curve and tint.

### The four code profiles

These exist in C++ and need no asset. `UTrackMarkSubsystem::GetBuiltInProfile` creates them on first use.

| | Boot | Paw | Tyre | Track |
|---|---|---|---|---|
| Size (cm, along × across) | 26 × 11 | 12 × 10 | 40 × 22 | 60 × 50 |
| Opacity | 0.75 | 0.55 | 0.80 | 0.85 |
| Variants | 2 | 2 | 1 | 1 |
| Lifetime (s) | 25 | 18 | 45 | 60 |
| Lifetime jitter | 0.15 | 0.20 | 0.08 | 0.05 |
| Fade starts at | 55 % | 40 % | 65 % | 70 % |
| Decay exponent | 1.6 | 1.3 | 2.0 | 2.4 |
| Lateral spacing (cm) | 14 | 10 | 150 | 260 |
| Stride (cm) | 75 | 55 | 22 | 28 |
| Max slope (°) | 45 | 50 | 40 | 40 |
| Yaw jitter (°) | 4 | 8 | 0 | 0 |
| Mirrors left side | yes | yes | no | no |

Tyre and Track use a short stride on purpose: a wheel lays down a continuous band, so consecutive marks
are meant to overlap into a line rather than read as separate stamps.

### Making your own

Content Browser → **Miscellaneous → Data Asset → TrackMark Profile**.

Notable fields beyond the table above:

- `Mesh` — must be flat in XY, facing +Z, pivot centred. Placement scales X along the direction of
  travel and Y across it.
- `Material` — your own master material. It only has to read the four custom data floats (§7).
- `SurfaceOffset` — lift above the traced surface point, in centimetres. The first defence against
  Z-fighting.
- `LocalOffset` — extra offset in the mark's own space, X forward / Y right / Z up.
- `bAlignToSurfaceNormal` — off keeps the mark horizontal, which reads badly on ramps.
- `NumVariants` — how many shapes the material may draw, fed to it through the custom data.
- `ScalarParameters` / `VectorParameters` — anything else your material wants. Pushed onto the batch's
  material instance when the batch is created; names the material does not know are ignored.

---

## 5. Surfaces and physical materials

The placement trace runs with `bReturnPhysicalMaterial = true`. The `UPhysicalMaterial` on the hit is
looked up in a map and picks the profile.

- **Project-wide:** *Project Settings → Plugins → TrackMark → Surface Profiles*.
- **At runtime, per world:** `UTrackMarkStatics::SetTrackMarkProfileForSurface`.

A surface that is not in the map, or a hit with no physical material at all, falls through to the
default profile. That is **not** an error and is not logged — a level full of untagged geometry still
produces marks.

Resolution order for one placement:

1. The global override, if one is set (`SetTrackMarkProfileOverride` — this is what a debug button uses).
2. The surface's physical material, if the request allows it (`bAllowSurfaceProfile`, which
   `UTrackMarkComponent::bUseSurfaceProfiles` controls).
3. The profile named in the request — for a component, its `Profile` asset or its `BuiltInProfile`.
4. The project's default profile, and failing that the fallback code profile.

### Slope

The angle between the hit normal and world up is compared against the profile's `MaxSlopeAngle`. Above
it the request returns `TooSteep` and is counted in `Rejected (slope)`. Below it, the mark is rotated so
its up axis is the surface normal — a footprint on a ramp lies **on** the ramp.

---

## 6. The budget and recycling

`MarkBudget` is a hard cap on live marks per world. It does not stop new marks; it decides what happens
to the old ones.

Live marks are held in a fixed-size ring buffer in placement order. On placement:

- **Under budget:** the batch hands out a free instance slot if it has one (`recycled` goes up), or
  allocates a new one (`allocated` goes up). Allocation only happens during warm-up.
- **At budget:** the world's oldest mark is retired first — its instance is collapsed to nothing and its
  slot returned to its batch's free list — and the new mark takes a slot from the free list. `recycled`
  goes up, `allocated` does not.

Instance slots are **never removed** from a component. Removing an instance swaps the last one into the
hole, which would invalidate every slot index above it, and this system hands out indices by the
thousand. A retired instance is collapsed to a 1e-4 scale instead, so it rasterises nothing.

Lowering the budget at runtime (`SetTrackMarkBudget`, `TrackMark.Budget 256`, or the HUD button) retires
the oldest marks immediately. The count falls, the draw call count does not move, and no instance is
freed — so raising the budget again reuses the same slots.

Resizing the ring is the one operation that allocates on purpose. It happens on a budget change, never
during play.

### The one thing that does grow

Slots are per batch. If marks migrate between profiles — say a demo cycles Boot → Paw → Tyre → Track —
each batch keeps the slots it once needed, so the total allocated slot count can exceed the budget while
the *live* count never does. This is why the overlay shows both numbers.

---

## 7. The master material

`M_TrackMark` is drawn **procedurally**. There is no painted texture anywhere in this plugin.

### The custom data contract

Every batch component is created with four per-instance custom data floats. Any material used with
TrackMark must read them with the **PerInstanceCustomData** node:

| Index | Meaning |
|---|---|
| 0 | `SpawnTime` — world seconds when the mark was placed |
| 1 | `InvLifetime` — 1 / lifetime in seconds; **0 means the mark never fades** |
| 2 | `Opacity` — peak opacity, already scaled by the profile and the placement request |
| 3 | `VariantSigned` — `abs(x) - 1` is the variant index, `sign(x)` is the mirror flag (negative = mirrored) |

Index 3 packs two things because four floats is the whole budget.

### Age

```
NormalisedAge = saturate( (Time - CustomData0) * CustomData1 )
```

`Time` is the material's Time input. A mark is written once when it is placed and never touched again,
which is the whole reason there is no CPU ageing loop.

Note that `InvLifetime = 0` makes `NormalisedAge` collapse to 0 forever, which is exactly what "never
fades" should do — no special case needed.

### Fade

```
FadeAlpha = saturate( (NormalisedAge - FadeStart) / max(1 - FadeStart, 0.001) )
Fade      = pow( 1 - FadeAlpha, DecayExponent )
FinalOpacity = CustomData2 * Shape * Fade
```

`FadeStart` and `DecayExponent` are scalar parameters, set per profile on the batch's material instance.

### The shape

Work in centred coordinates: `P = TexCoord0 * 2 - 1`, so `P.x` runs along the direction of travel and
`P.y` across it, both in −1…1.

Mirroring: `P.x = P.x * sign(CustomData3)`. (Mirror across the length axis, because the pair of feet is
mirrored across the movement line.)

An ellipse mask is one node group used three times:

```
Ellipse(P, Centre, Radii) = 1 - smoothstep(0.85, 1.0, length((P - Centre) / Radii))
```

- **Boot** — `max( Ellipse(P, (0.30, 0), (0.62, 0.92)), Ellipse(P, (-0.52, 0), (0.40, 0.70)) )`.
  A ball and a heel, overlapping slightly. That is a recognisable boot print with two ellipses.
- **Paw** — the same two ellipses with the heel pulled in and rounded: centres `(0.20, 0)` and
  `(-0.40, 0)`, radii `(0.70, 0.85)` and `(0.45, 0.60)`.
- **Tyre** — a rounded rectangle, `1 - smoothstep(0.85, 1, max(|P.x| / 0.95, |P.y| / 0.8))`, multiplied
  by a lateral rib pattern: `step(0.45, frac(P.x * 5))` softened with a small smoothstep.
- **Track** — the same rounded rectangle with a cleat pattern across the width instead of along it:
  `frac(P.y * 4)`, plus two solid side rails at `|P.y| > 0.75`.

Variants come from `abs(CustomData3) - 1`: variant 1 rotates `P` by a few degrees and scales the heel
ellipse down slightly. That is enough to stop a straight walk looking stamped, and it costs two nodes.

### Material settings

- **Material Domain:** Surface
- **Blend Mode:** Translucent
- **Shading Model:** Unlit
- **Two Sided:** off
- **Used with Instanced Static Meshes:** on
- **Emissive Color:** the `Tint` vector parameter
- **Opacity:** `FinalOpacity` from above

Unlit and translucent is the cheap, predictable choice: the mark tints the ground it lies on and costs
no lighting work. If you want marks that pick up scene lighting, switch the shading model to Default Lit
and drive Base Color from the tint — the custom data contract does not change.

---

## 8. The stats overlay

`ATrackMarkHUD` is an `AHUD` subclass that draws on `UCanvas`. Canvas rather than UMG on purpose: it
draws in a cooked **Shipping** build, where `DrawDebug` is compiled out and a debug widget is usually
stripped.

Set it as the HUD class on your Game Mode.

### The counters

| Line | Meaning |
|---|---|
| `Active marks` | Live marks, and the budget |
| `Draw calls` | Batches holding at least one live mark, and the total batch count |
| `Instance slots` | Slots allocated across all batches, and how many are free |
| `Slots recycled` | Cumulative placements that reused a slot — this is the number that rises when the budget is lowered |
| `Slots allocated` | Cumulative placements that had to allocate. Flat after warm-up |
| `Traces this frame` | Surface traces performed last frame |
| `Placed / retired` | Marks placed and retired last frame |
| `Rejected (slope)` | Requests refused because the ground was too steep |
| `Subsystem tick` | Milliseconds the subsystem spent in its own tick |
| `Placement` | Milliseconds spent inside placement calls, traces included |

Placement happens during actor ticks, which can run either side of the subsystem's. The overlay
therefore shows the **previous** frame's accumulators, so the numbers stay honest whatever the tick
order.

### The buttons

Four built-in buttons — `PROFILE`, `BUDGET`, `STATS`, `CLEAR MARKS` — are `AHUD` hit boxes and answer to
real mouse clicks. `bAutoEnableMouseInput` turns on `bShowMouseCursor` and `bEnableClickEvents` on the
owning player controller at BeginPlay, because a hit box without click events is a button that does
nothing.

Every label is derived from the subsystem **on the frame it is drawn** and never cached, so the panel
cannot claim one thing while the system does another — the very first click always does what the label
says. The `PROFILE` button steps from what the subsystem is actually doing rather than from a counter of
its own, so it stays in step even when something else changes the override.

Add your own with `AddButton(Id, Label, Accent)` and `SetButtonLabel(Id, Label)`. Built-in and custom
buttons fire the same `OnButtonClicked` delegate, so one Blueprint handler can drive the lot.

---

## 9. Blueprint reference

Everything below is on `UTrackMarkStatics` unless noted. Nothing in this plugin needs a line of C++.

### Placement

| Node | Notes |
|---|---|
| `Leave Track Mark` | One mark at a world location, facing a direction |
| `Leave Track Mark For Actor` | One mark under an actor, facing the way it moves; the actor is excluded from the trace |
| `Leave Track Mark From Request` | Full control through `FTrackMarkRequest` |
| `Clear All Track Marks` | Retires everything. One bulk write per batch |

### Budget and state

`Set Track Mark Budget`, `Get Track Mark Budget`, `Set Track Marks Enabled`, `Are Track Marks Enabled`,
`Get Track Mark Stats`.

### Profiles

`Get Built In Track Mark Profile`, `Get Default Track Mark Profile`, `Set Default Track Mark Profile`,
`Set Track Mark Profile For Surface`, `Set Track Mark Profile Override`, `Get Track Mark Profile
Override`, `Get Built In Track Mark Profile Name`.

### Utility

`Get Track Mark Subsystem`, `Spawn Test Track Marks`.

### On the component

`Leave Track`, `Leave Track At`, `Leave Track Alternating`, `Set Trigger Mode`, `Reset Stride`,
`Get Effective Profile`, and the `On Track Mark Placed` event.

### On the settings

`Set Track Marks Enabled`, `Set Default Mark Budget`, `Set Max Retirements Per Tick`,
`Set Surface Trace Channel`, `Set Fallback Built In Profile`. These write to the in-memory defaults and
**never** touch the `.ini`, so an options-menu slider cannot corrupt the shipped configuration.

---

## 10. Project settings

*Project Settings → Plugins → TrackMark*, stored in `DefaultGame.ini`.

| Setting | Default | Notes |
|---|---|---|
| `Enable Track Marks` | on | Off refuses new marks and creates no batch components |
| `Mark Budget` | 2048 | Hard cap on live marks per world |
| `Default Profile` | none | Empty uses the fallback code profile |
| `Fallback Built In Profile` | Boot | Why a fresh install already leaves footprints |
| `Default Mark Mesh` | `/Engine/BasicShapes/Plane` | Must be flat in XY, +Z, pivot centred |
| `Default Mark Material` | `M_TrackMark` | Empty or missing falls back to the mesh's material |
| `Reference Mesh Size` | (100, 100) | Size of the default mesh in cm at scale 1 |
| `Surface Profiles` | empty | Physical material → profile |
| `Surface Trace Channel` | `WorldStatic` | |
| `Trace Complex` | off | Right for landscapes and almost everything else |
| `Max Retirements Per Tick` | 256 | Keeps one frame bounded after a long pause |
| `Batch Bounds Scale` | 2.0 | Culling headroom; instances sit far from the batch origin |
| `Instance Start / End Cull Distance` | 0 | 0 disables distance culling |
| `Tick In Editor Worlds` | on | See §13 |

---

## 11. Console commands and variables

```
TrackMark.Test [Count] [Radius]   scatter marks in a disc in front of the local viewpoint
TrackMark.Budget [Count]          print, or set, the hard cap on live marks
TrackMark.Clear                   retire every live mark at once
TrackMark.Stats                   print the measured counters to the log
TrackMark.Enabled 0|1             console variable; refuses new marks, existing ones keep fading
```

`TrackMark.Test` reports how many marks actually landed. A mark over a hole or a wall is refused, not
faked — if 500 were requested and 480 placed, 20 found no ground or too steep a slope.

---

## 12. Performance notes

**Per mark, once:** one line trace, one `UpdateInstanceTransform`, one `SetCustomData`. Both writes pass
`bMarkRenderStateDirty = false`; the instance data manager tracks the change and streams it to the GPU
scene. Recreating the render state would cost the whole batch on every footstep.

**Per frame:** the retirement sweep walks the oldest end of the ring and stops at the first mark that is
still alive, capped at `Max Retirements Per Tick`. Nothing else runs.

**Allocation:** after warm-up, none. The ring is fixed size, the scratch buffers are reused, instance
slots are recycled. The only planned allocation is resizing the ring on a budget change.

**Batch components** are created with shadow casting, contact shadows, indirect lighting, distance field
lighting, occluder use, decal receiving, collision, overlaps and navigation relevance all switched off.
Navigation in particular matters: thousands of navigation-relevant instances would rebuild the navmesh.

**The trace is the real cost.** It is the only per-mark work that scales with scene complexity, which is
why it is counted separately in the overlay. If you already know the ground — a flat arena, a fixed
floor height — set `bTraceForSurface = false` on the request and placement becomes two writes.

---

## 13. Limits and known caveats

**The decay clock.** The material derives a mark's age from the scene's `Time` input and the spawn time
written into custom data 0, which comes from `UWorld::GetTimeSeconds()`. These agree in Game and PIE.
The subsystem can also run in a plain **editor world** (`Tick In Editor Worlds`, on by default), where
the two clocks are the editor world's rather than a game world's; this path has not been measured and
should be treated as untested. If marks placed from an editor tool age oddly, switch the setting off and
place them in PIE.

**Draw calls grow with profiles in use.** One profile is one batch is one draw call. Cycling through
four profiles leaves four batches in existence, and each keeps the instance slots it once needed. Both
numbers are on the overlay so this is visible rather than surprising.

**No replication.** Marks are local and cosmetic. Two clients will not see identical footprints, and
that is deliberate — replicating a cosmetic that can appear a hundred times a second is not a trade
worth making.

**Translucent overdraw.** Marks are translucent quads lying on the ground. Two thousand of them piled in
one spot is two thousand layers of overdraw in that spot. The budget is the control for this; the
per-profile cull distances are the other.

**Mesh requirements.** The mark mesh must be a flat XY plane facing +Z with a centred pivot. A cube, or
a plane with an off-centre pivot, will place and scale wrongly.

**Physics-driven surfaces.** The trace uses one collision channel. A surface that does not block that
channel is invisible to the system and returns `NoGround`.

---

## 14. Troubleshooting

**No marks at all.**
Check the overlay or `TrackMark.Stats`. If `Traces this frame` is 0, nothing is calling placement — the
component is in Manual mode with no notify, or `MinSpeed` is above the actor's speed. If traces are
happening but nothing is placed, the returns are `NoGround` (the trace channel does not hit your floor,
or the trace is too short for a component mounted high on the actor) or `TooSteep`.

**Marks are opaque grey rectangles.**
The master material could not be loaded and the batch fell back to the mesh's own material. There is one
warning in the log naming the path. Install the plugin's content, or point *Default Mark Material* at
your own material that reads the custom data.

**Marks never fade.**
Either the material is not reading custom data 0 and 1, or the profile's `Lifetime` is 0, which means
"never ages out" on purpose. Such marks still get recycled once the budget comes round to them.

**Marks float above or sink into the ground.**
Adjust the profile's `SurfaceOffset`. Too small gives Z-fighting, too large gives a visible gap on
slopes. 1 cm is the default and is right for most scales.

**Marks are doubled up side by side.**
The notify has a `SocketName` set *and* the profile has a large `LateralSpacing`. When a socket is used
the mark is centred on it, so this should not happen — but a manual `LeaveTrack(Left)` from a
foot-mounted component will apply the spacing on top of the socket's own offset. Set the profile's
`LateralSpacing` to 0 for foot-socket setups.

**The HUD buttons do nothing.**
Hit boxes need click events on the player controller. Leave `bAutoEnableMouseInput` on, or set
`bShowMouseCursor` and `bEnableClickEvents` yourself. If your game sets an input mode of
*Game Only* after BeginPlay, it will take the cursor back.

**Draw calls keep climbing.**
Each distinct profile in use is a batch. If something is creating profile assets at runtime, every one
is a new batch. Reuse profiles.

---

## Support

Silvan Teufel — <teufelsilvan@gmail.com> — <https://silvan.teufel-engineering.com>

# TrackMark — Footprints, Tracks & Tyre Marks

Ground tracks that cost one draw call.

Footprints, paw prints, tyre marks and tank tracks that lie **on** the surface, tilt with its normal,
change shape and opacity with the physical material under the foot, and fade out on their own.

Every mark of one profile is an instance in a single instanced static mesh — not a decal component per
print. One decal per footprint is one draw call per footprint, and that is where hand-rolled footprint
systems fall over at a few hundred marks.

Ageing runs in the material from four per-instance custom data floats, so no CPU loop walks the live
marks every frame. A hard budget caps the count: over the cap the oldest mark is recycled into the new
one, so after warm-up the runtime allocates nothing at all.

Four built-in code profiles — Boot, Paw, Tyre, Track — mean marks appear before you have authored a
single asset.

---

## Quick start

1. Enable the plugin.
2. Add a **Track Mark** component to your character. Attach it to the root, or to a foot socket.
3. Press Play and walk.

That is the whole setup. The component defaults to distance triggering with the built-in Boot profile,
so it needs no animation notify, no data asset and no project configuration.

For frame-accurate footfalls, switch the component's **Trigger Mode** to `Manual / Anim Notify` and put
the **Track Mark** notify on the foot-plant frames of your walk and run animations.

---

## What it does

- **One draw call per profile.** All marks of a profile live in one `UInstancedStaticMeshComponent`.
- **Written once.** A mark costs one transform and four custom data floats, then the CPU forgets it.
- **Material-driven decay.** The material derives each mark's age from its spawn time and the scene
  clock. Ten thousand live marks cost zero CPU per frame.
- **A hard budget.** Over the cap the oldest mark is retired and its instance slot handed straight to the
  new one. The `recycled` counter goes up; the `allocated` counter does not.
- **Surface aware.** A downward trace reads the `UPhysicalMaterial` under the foot and maps it to a
  profile. Grass, sand and snow can differ in shape, size, opacity and lifetime.
- **Slope aware.** Marks lie along the surface normal, and ground steeper than the profile's limit is
  refused rather than plastered with a horizontal footprint.
- **No painted textures.** The master material draws its shapes procedurally.
- **A real overlay.** `ATrackMarkHUD` draws the counters and clickable controls on `UCanvas`, so it
  survives a cooked Shipping build.

## What it does not do

- No terrain deformation. Marks are drawn, not dug.
- No footstep audio.
- No characters, no animations, no meshes of your own to replace.
- No network replication. Marks are local and cosmetic by design.
- No tracking or trail-reading gameplay.

---

## Console commands

| Command | What it does |
|---|---|
| `TrackMark.Test [Count] [Radius]` | Scatters marks in a disc in front of the local viewpoint. |
| `TrackMark.Budget [Count]` | Prints, or sets, the hard cap on live marks. |
| `TrackMark.Clear` | Retires every live mark at once. Slots stay allocated for reuse. |
| `TrackMark.Stats` | Prints the measured counters to the log. |
| `TrackMark.Enabled 0/1` | Console variable. Refuses new marks; existing ones keep fading. |

---

## Platforms

Built and verified on **Win64** with Unreal Engine 5.8. **Mac** and **Linux** are allow-listed in the
`.uplugin` but have not been built or tested.

---

## Documentation

Full documentation, including the master material's node graph and the custom data contract, is in
[`Docs/DOCUMENTATION.md`](Docs/DOCUMENTATION.md).

---

## Support

Silvan Teufel — <teufelsilvan@gmail.com> — <https://silvan.teufel-engineering.com>

<!-- SF-STORE-BLOCK:BEGIN -->
## 🛒 Source-available — see before you buy

This repository contains the **full source** of a commercial Unreal Engine plugin. It is **source-available, not open source**: read it, evaluate it, then buy a license to use it. See **the Fab Content License Agreement / Unreal Engine EULA (purchase required)**.

**Get it / Buy:**
- Fab store — all our UE5 plugins: https://www.fab.com/sellers/Silvan%20Teufel

_This plugin does not have its own Fab listing yet — the store link above is where everything we currently sell lives._

### 📬 **Free UE5 Snippet-Pack**

10 ready-to-use C++/Blueprint building blocks (subsystems, versioned saves, async nodes, editor tooling) — MIT licensed. Get it by joining the newsletter — plus a heads-up when something new ships. Double opt-in, unsubscribe in one click, no address sharing.

👉 **[Get the free pack](https://silvan.teufel-engineering.com/newsletter/plugins/?q=gh)**

_© 2026 Silvan Teufel. All rights reserved._
<!-- SF-STORE-BLOCK:END -->

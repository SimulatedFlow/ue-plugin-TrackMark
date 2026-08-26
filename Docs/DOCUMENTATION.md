# TrackMark — Documentation

**Ground tracks that cost one draw call.**
Footprints, paw prints, tyre marks and tank tracks — pooled, surface-aware, budgeted.

| | |
|---|---|
| **Plugin version** | 1.0.0 |
| **Engine** | Unreal Engine **5.8** (`EngineVersion: "5.8.0"`) |
| **Modules** | one runtime module, `TrackMark`, `LoadingPhase: PreDefault`. No editor module. |
| **Platforms** | **Win64 — built and verified.** Mac and Linux are **not** in the `.uplugin`'s `PlatformAllowList` and have not been built or tested. |
| **Dependencies** | `Core`, `CoreUObject`, `Engine`, `DeveloperSettings`, `RenderCore`, `PhysicsCore` — engine modules only. No UMG, no Niagara, no AIModule, no other Marketplace plugin. |
| **Replication** | none. Marks are local and cosmetic by design. |
| **C++ required** | no. The whole surface is exposed to Blueprint. |

---

## Contents

1. [The idea](#1-the-idea)
2. [Supported platforms and engine](#2-supported-platforms-and-engine)
3. [Installation](#3-installation)
4. [Quick start](#4-quick-start)
5. [Class and API overview](#5-class-and-api-overview)
6. [Code examples](#6-code-examples)
7. [Triggering marks](#7-triggering-marks)
8. [Profiles](#8-profiles)
9. [Surfaces and physical materials](#9-surfaces-and-physical-materials)
10. [The budget and recycling](#10-the-budget-and-recycling)
11. [The master material](#11-the-master-material)
12. [The stats overlay](#12-the-stats-overlay)
13. [Blueprint reference](#13-blueprint-reference)
14. [Project settings](#14-project-settings)
15. [Console commands and variables](#15-console-commands-and-variables)
16. [Demo content](#16-demo-content)
17. [Performance notes](#17-performance-notes)
18. [Limits and known caveats](#18-limits-and-known-caveats)
19. [Troubleshooting](#19-troubleshooting)
20. [Support](#20-support)

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

## 2. Supported platforms and engine

**Engine:** Unreal Engine **5.8**. The plugin targets `5.8.0` in its `.uplugin` and has not been
back-ported to earlier versions. Nothing in the source is 5.8-exclusive in an obvious way, but no other
version has been compiled, so no other version is claimed.

**Target platforms:**

| Platform | Status |
|---|---|
| **Win64** | **Built and verified.** Development Editor compiled, demo map run in Standalone PIE at 1920×1032, counters read off the live overlay. |
| **Mac** | Not in the `PlatformAllowList`. **Not built, not tested.** |
| **Linux** | Not in the `PlatformAllowList`. **Not built, not tested.** |
| Consoles / mobile / VR | **Not allow-listed.** Not claimed, not supported. |

The `PlatformAllowList` on the single module reads `["Win64"]`. There is nothing
platform-specific in the code — no intrinsics, no platform headers, no `#if PLATFORM_WINDOWS` — so Mac
and Linux are expected to build cleanly. "Expected" is not "verified", and this table says so on
purpose.

**Build configurations:** the module is `Runtime` type, so it is present in Development, Debug, Test
and **Shipping**. The stats overlay draws on `UCanvas` from `AHUD` rather than in UMG or through
`DrawDebug*`, specifically so it still works in a cooked Shipping build.

**Renderer:** works with Deferred and Forward shading. The mark material is Unlit/Translucent, which is
the cheapest and most predictable choice on both. Nanite and Lumen are irrelevant to the system — the
mark mesh is a two-triangle plane.

---

## 3. Installation

### From Fab

1. Install the plugin to your engine version from the Epic Games Launcher (**Library → Vault → Install
   to Engine**).
2. Open your project → **Edit → Plugins**, search for *TrackMark*, tick **Enabled**.
3. Restart the editor when prompted.

### Into a project

1. Copy the `TrackMark` folder into your project's `Plugins/` directory, so you have
   `<YourProject>/Plugins/TrackMark/TrackMark.uplugin`.
2. **C++ projects:** right-click the `.uproject` → **Generate Visual Studio project files**, then build.
   The plugin compiles as part of your project.
3. **Blueprint-only projects:** the plugin ships with prebuilt binaries for the engine version it was
   installed for. If the editor asks to rebuild it and cannot, the project has no C++ toolchain — add
   any empty C++ class to the project once and the toolchain is set up.
4. Enable it under **Edit → Plugins** and restart.

### Verifying the install

Open the output log and type into the console:

```
TrackMark.Stats
```

If the subsystem is alive you get a block of counters. If you get
`TrackMark.Stats: no TrackMark subsystem in this world`, the plugin is loaded but the current world does
not support the subsystem — that happens in a world type outside Game, PIE and Editor.

### Packaging

Nothing to do. The module is `Runtime`, it is included automatically, and the plugin's content
(`Content/TrackMark/`) is cooked with the rest of your project. If you never reference the demo content,
exclude the folder from cooking to save the space — the plugin does not need its own demo map to
function, only the master material `M_TrackMark`.

---

## 4. Quick start

Three steps, no assets, no configuration.

1. **Enable the plugin.**
2. Open a character Blueprint → **Add Component → Track Mark**.
3. **Play.**

You will see boot prints behind the character. Nothing else is required: the component falls back to a
**code profile** that exists in C++, and the mesh falls back to the engine's own
`/Engine/BasicShapes/Plane`.

### Making it better, in the order that pays off

| Step | What it buys |
|---|---|
| Attach the component to a **foot socket** instead of the actor root | Prints under the feet rather than under the actor's centre |
| Switch **Trigger Mode** to `Manual / Anim Notify` and add the **Track Mark** notify on foot-plant frames | Frame-accurate footfalls instead of a distance guess |
| Set the **HUD class** on your Game Mode to `ATrackMarkHUD` | The counters and the clickable controls, in-game and in Shipping |
| Create **TrackMark Profile** data assets and map them to your **physical materials** | Snow, sand and mud that look different |
| Set the **Mark Budget** in Project Settings | The cap that decides your memory and overdraw ceiling |

### The 60-second demo

```
TrackMark.Test 2000 900     # scatter 2000 marks in front of the camera
TrackMark.Stats             # one batch, one draw call, 2000 active
TrackMark.Budget 256        # the count falls, the draw calls do not, recycled climbs
TrackMark.Clear             # instant, no hitch, slots stay allocated
```

That sequence is the whole product argument, and it runs in any level with a floor.

---

## 5. Class and API overview

Seven public classes, plus two structs and four enums. Everything with a `U`/`A` prefix is a `UObject`
and visible to Blueprint.

| Class | Kind | What it is for |
|---|---|---|
| `UTrackMarkComponent` | `USceneComponent` | Hang it on anything that should leave marks. Its own transform is the foot position. |
| `UTrackMarkProfile` | `UDataAsset` | Look **and** behaviour of one kind of mark. Also the batching key. |
| `UTrackMarkSubsystem` | `UTickableWorldSubsystem` | Owns the instance pools, the budget, the ageing and the numbers. One per world. |
| `UTrackMarkStatics` | `UBlueprintFunctionLibrary` | The complete Blueprint surface. Every function resolves the subsystem itself. |
| `UTrackMarkSettings` | `UDeveloperSettings` | Project-wide defaults, plus runtime setters that never touch the `.ini`. |
| `ATrackMarkHUD` | `AHUD` | Canvas stats overlay and clickable control panel. Works in Shipping. |
| `UAnimNotify_TrackMark` | `UAnimNotify` | Drops a mark at the exact animation frame the foot lands. |

### Types

| Type | Purpose |
|---|---|
| `FTrackMarkRequest` (struct) | Everything one placement needs: location, forward, profile, side, trace settings, opacity/size/lifetime scales. Defaults alone already produce a sensible boot print. |
| `FTrackMarkStats` (struct) | Fourteen measured counters. Read it from Blueprint or draw it with `ATrackMarkHUD`. |
| `ETrackMarkBuiltInProfile` | `Boot`, `Paw`, `Tyre`, `Track` — the four code profiles. |
| `ETrackMarkSide` | `Left`, `Right`, `Center`. Drives lateral offset and mirroring. |
| `ETrackMarkTriggerMode` | `Manual` (no tick at all) or `Distance`. |
| `ETrackMarkResult` | `Placed`, `Disabled`, `NoGround`, `TooSteep`, `NoProfile`, `NoMesh`, `NoSlot`. Nothing fails silently. |

### The hot path, end to end

```
UTrackMarkComponent::LeaveTrack()          // or the anim notify, or a Blueprint call
  → BuildRequest()                          // fills FTrackMarkRequest from the component
  → UTrackMarkSubsystem::PlaceMark()
      → line trace down (bReturnPhysicalMaterial = true)   ← the only notable cost
      → resolve profile: override → surface → request → default
      → slope check against Profile->MaxSlopeAngle
      → FindOrCreateBatch(Profile)          // one batch = one ISM = one draw call
      → AcquireSlot()                       // recycle the oldest if at budget
      → UpdateInstanceTransform() + SetCustomData()   // both with bMarkRenderStateDirty = false
  → returns ETrackMarkResult
```

The subsystem's own tick does exactly two things: retire marks whose lifetime has run out (capped at
`MaxRetirementsPerTick`), and publish the stats. Both are bounded.

### Key functions at a glance

**`UTrackMarkStatics`** — placement: `LeaveTrackMark`, `LeaveTrackMarkForActor`,
`LeaveTrackMarkFromRequest`, `ClearAllTrackMarks`, `SpawnTestTrackMarks`. Budget and state:
`SetTrackMarkBudget`, `GetTrackMarkBudget`, `SetTrackMarksEnabled`, `AreTrackMarksEnabled`,
`GetTrackMarkStats`. Profiles: `GetBuiltInTrackMarkProfile`, `GetDefaultTrackMarkProfile`,
`SetDefaultTrackMarkProfile`, `SetTrackMarkProfileForSurface`, `SetTrackMarkProfileOverride`,
`GetTrackMarkProfileOverride`, `GetBuiltInTrackMarkProfileName`. Utility: `GetTrackMarkSubsystem`.

**`UTrackMarkComponent`** — `LeaveTrack`, `LeaveTrackAt`, `LeaveTrackAlternating`, `SetTriggerMode`,
`ResetStride`, `GetEffectiveProfile`, and the `OnTrackMarkPlaced` multicast delegate.

**`UTrackMarkSubsystem`** — `Get`, `PlaceMark` / `PlaceTrackMark`, `ClearAllMarks`, `SetMarkBudget`,
`GetMarkBudget`, `GetBuiltInProfile`, `GetDefaultProfile`, `SetDefaultProfile`, `SetProfileForSurface`,
`GetProfileForSurface`, `SetProfileOverride`, `GetProfileOverride`, `SetMarksEnabled`, `AreMarksEnabled`,
`GetStats`, `LogStats`, `SpawnTestMarks`.

**`ATrackMarkHUD`** — `AddButton`, `SetButtonLabel`, `RemoveButton`, `ClearButtons`, the
`OnButtonClicked` delegate, and the four built-in button ids `ButtonId_Profile`, `ButtonId_Budget`,
`ButtonId_Stats`, `ButtonId_Clear`.

---

## 6. Code examples

All examples are C++. Every one of them has a one-to-one Blueprint equivalent — see §13.

### Module dependency

Add the module to your `Build.cs` before including anything:

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "TrackMark" });
```

### 6.1 A character that leaves footprints

```cpp
// MyCharacter.h
#include "GameFramework/Character.h"
#include "MyCharacter.generated.h"

class UTrackMarkComponent;

UCLASS()
class AMyCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AMyCharacter();

private:
    UPROPERTY(VisibleAnywhere, Category = "TrackMark")
    TObjectPtr<UTrackMarkComponent> TrackMarks;
};
```

```cpp
// MyCharacter.cpp
#include "MyCharacter.h"
#include "TrackMarkComponent.h"

AMyCharacter::AMyCharacter()
{
    TrackMarks = CreateDefaultSubobject<UTrackMarkComponent>(TEXT("TrackMarks"));
    TrackMarks->SetupAttachment(GetRootComponent());

    // Distance triggering needs no animation work at all.
    TrackMarks->TriggerMode      = ETrackMarkTriggerMode::Distance;
    TrackMarks->BuiltInProfile   = ETrackMarkBuiltInProfile::Boot;
    TrackMarks->bAlternateFeet   = true;
    TrackMarks->MinSpeed         = 40.0f;   // do not stamp while shuffling in place
}
```

That is a complete, working footprint setup. Everything below is refinement.

### 6.2 Frame-accurate footfalls from an animation notify

Attach one component per foot and let the notify pick the right one by name:

```cpp
LeftFootMarks  = CreateDefaultSubobject<UTrackMarkComponent>(TEXT("LeftFootMarks"));
LeftFootMarks->SetupAttachment(GetMesh(), TEXT("foot_l"));
LeftFootMarks->TriggerMode = ETrackMarkTriggerMode::Manual;   // no tick at all

RightFootMarks = CreateDefaultSubobject<UTrackMarkComponent>(TEXT("RightFootMarks"));
RightFootMarks->SetupAttachment(GetMesh(), TEXT("foot_r"));
RightFootMarks->TriggerMode = ETrackMarkTriggerMode::Manual;
```

Then in the animation editor, add the **Track Mark** notify on each foot-plant frame and set
`ComponentName` to `LeftFootMarks` / `RightFootMarks`. In `Manual` mode the component's tick function is
switched off entirely, so a thousand idle components cost nothing.

If you would rather drive it from your own notify or from a Blueprint anim graph:

```cpp
void AMyCharacter::OnFootPlant(bool bLeftFoot)
{
    UTrackMarkComponent* Foot = bLeftFoot ? LeftFootMarks : RightFootMarks;
    const ETrackMarkResult Result = Foot->LeaveTrack(
        bLeftFoot ? ETrackMarkSide::Left : ETrackMarkSide::Right);

    if (Result == ETrackMarkResult::NoGround)
    {
        // Airborne, or the trace channel does not hit this floor. Not an error.
    }
}
```

### 6.3 A one-off mark from anywhere

```cpp
#include "TrackMarkStatics.h"

// Under an actor, facing the way it moves; the actor is excluded from the trace.
UTrackMarkStatics::LeaveTrackMarkForActor(SomeActor);

// At an explicit location and direction.
UTrackMarkStatics::LeaveTrackMark(
    this,                       // any world context object
    ImpactLocation,
    ImpactDirection,
    /*Profile=*/ nullptr,       // null: resolve from the surface, then the default
    ETrackMarkSide::Center,
    /*IgnoreActor=*/ this);
```

### 6.4 Full control through the request struct

A heavy landing: bigger, darker, longer-lived than a walk step, and no lateral offset.

```cpp
#include "TrackMarkStatics.h"
#include "TrackMarkTypes.h"

FTrackMarkRequest Request;
Request.Location          = GetActorLocation();
Request.Forward           = GetActorForwardVector();
Request.Side              = ETrackMarkSide::Center;
Request.IgnoreActor       = this;
Request.OpacityScale      = 1.6f;    // stamps harder
Request.SizeScale         = 1.3f;    // and wider
Request.LifetimeScale     = 2.0f;    // and stays twice as long
Request.TraceDownDistance = 300.0f;  // component is mounted at the hip

const ETrackMarkResult Result =
    UTrackMarkStatics::LeaveTrackMarkFromRequest(this, Request);

switch (Result)
{
case ETrackMarkResult::Placed:    break;
case ETrackMarkResult::TooSteep:  /* landed on a wall; refuse rather than plaster it */ break;
case ETrackMarkResult::NoGround:  /* nothing under the feet within 300 cm */ break;
default:                          break;
}
```

If you already know the ground — a flat arena, a fixed floor height — set
`Request.bTraceForSurface = false` and placement collapses to two writes with no trace at all.

### 6.5 Surface-driven profiles

Map physical materials to profiles once, at startup, and every placement afterwards picks the right one
by itself:

```cpp
#include "TrackMarkStatics.h"

void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();

    UTrackMarkStatics::SetTrackMarkProfileForSurface(this, PM_Snow, DA_Profile_SnowBoot);
    UTrackMarkStatics::SetTrackMarkProfileForSurface(this, PM_Mud,  DA_Profile_MudBoot);
    // Anything not listed falls through to the default profile. That is not an error.
}
```

The same mapping can be authored project-wide, without code, under
**Project Settings → Plugins → TrackMark → Surface Profiles**.

### 6.6 Budget, quality settings and clearing

```cpp
#include "TrackMarkStatics.h"

// A graphics-options slider, in one line each.
void AMyOptionsMenu::ApplyTrackDetail(int32 Level)
{
    const int32 Budget = (Level == 0) ? 0 : (Level == 1 ? 256 : (Level == 2 ? 1024 : 4096));

    if (Budget == 0)
    {
        UTrackMarkStatics::SetTrackMarksEnabled(this, false);
        UTrackMarkStatics::ClearAllTrackMarks(this);
        return;
    }

    UTrackMarkStatics::SetTrackMarksEnabled(this, true);
    UTrackMarkStatics::SetTrackMarkBudget(this, Budget);
}
```

Lowering the budget retires the oldest marks immediately. The live count falls, the **draw call count
does not move**, and no instance slot is freed — so raising it again reuses the same slots.

### 6.7 Reading the numbers

```cpp
#include "TrackMarkStatics.h"
#include "TrackMarkTypes.h"

const FTrackMarkStats Stats = UTrackMarkStatics::GetTrackMarkStats(this);

UE_LOG(LogTemp, Display,
    TEXT("%d/%d marks in %d draw calls | %d slots (%d free) | recycled %d, allocated %d | %.2f ms"),
    Stats.ActiveMarks, Stats.Budget,
    Stats.ActiveBatches,
    Stats.InstanceSlots, Stats.FreeSlots,
    Stats.RecycledSlots, Stats.AllocatedSlots,
    Stats.TickMilliseconds + Stats.PlacementMilliseconds);
```

`AllocatedSlots` going flat while `RecycledSlots` keeps climbing is the proof that the system has warmed
up and stopped allocating.

### 6.8 Reacting to a placement

```cpp
// In BeginPlay:
TrackMarks->OnTrackMarkPlaced.AddDynamic(this, &AMyCharacter::HandleTrackMarkPlaced);

void AMyCharacter::HandleTrackMarkPlaced(FVector Location, ETrackMarkSide Side)
{
    // Cosmetic hook. Spawn a dust puff, play a sound, notify an AI tracker.
    // Fires only on a successful placement, and only locally.
}
```

### 6.9 Your own buttons on the overlay

```cpp
// In a subclass of ATrackMarkHUD, or on a Blueprint child of it:
void AMyHUD::BeginPlay()
{
    Super::BeginPlay();

    AddButton(TEXT("Demo.AddWalkers"), TEXT("+10 WALKERS"), FLinearColor(0.4f, 0.85f, 0.4f, 1.0f));
    AddButton(TEXT("Demo.Surface"),    TEXT("SURFACE: GRASS"));

    OnButtonClicked.AddDynamic(this, &AMyHUD::HandleButton);
}

void AMyHUD::HandleButton(FName ButtonId)
{
    if (ButtonId == TEXT("Demo.AddWalkers"))
    {
        SpawnWalkers(10);
    }
    // The four built-in buttons fire this same delegate, so one handler can drive the lot.
}
```

### 6.10 Direct subsystem access

When you want to hold a reference instead of passing a world context around:

```cpp
#include "TrackMarkSubsystem.h"

if (UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(this))
{
    Subsystem->SetProfileOverride(Subsystem->GetBuiltInProfile(ETrackMarkBuiltInProfile::Tyre));
    Subsystem->SetMarkBudget(4096);
    Subsystem->LogStats();
}
```

`UTrackMarkSubsystem::Get` returns null outside a world that supports the subsystem, and every
`UTrackMarkStatics` function fails quietly in the same situation — calling them from a menu level does
nothing rather than crashing.

---

## 7. Triggering marks

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

## 8. Profiles

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

> **Shape caveat.** The four **code** profiles set no `ScalarParameters`, so they do not send the
> master material a `ShapeType`, and `M_TrackMark` draws its default — the **boot** shape — for all
> four. Their *sizes, lifetimes, opacities, strides and spacings* are correct and distinct; only the
> silhouette is shared. The four shipped profile **assets**
> (`DA_TrackMarkProfile_Boot / Paw / Tyre / Track`) do send `ShapeType`, so they render the right shape.
> Use the assets when the silhouette matters; use the code profiles when you want marks with zero setup.
> See §18.

### Making your own

Content Browser → **Miscellaneous → Data Asset → TrackMark Profile**.

Notable fields beyond the table above:

- `Mesh` — must be flat in XY, facing +Z, pivot centred. Placement scales X along the direction of
  travel and Y across it.
- `Material` — your own master material. It only has to read the four custom data floats (§11).
- `SurfaceOffset` — lift above the traced surface point, in centimetres. The first defence against
  Z-fighting.
- `LocalOffset` — extra offset in the mark's own space, X forward / Y right / Z up.
- `bAlignToSurfaceNormal` — off keeps the mark horizontal, which reads badly on ramps.
- `NumVariants` — how many shapes the material may draw, fed to it through the custom data.
- `ScalarParameters` / `VectorParameters` — anything else your material wants. Pushed onto the batch's
  material instance when the batch is created; names the material does not know are ignored. This is
  where `ShapeType` is set for the shipped profile assets.

---

## 9. Surfaces and physical materials

The placement trace runs with `bReturnPhysicalMaterial = true`. The `UPhysicalMaterial` on the hit is
looked up in a map and picks the profile.

- **Project-wide:** *Project Settings → Plugins → TrackMark → Surface Profiles*.
- **At runtime, per world:** `UTrackMarkStatics::SetTrackMarkProfileForSurface` (§6.5).

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

## 10. The budget and recycling

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

## 11. The master material

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

**Selecting the shape.** In the shipped `M_TrackMark` the four shapes live in a single **Custom** HLSL
node and are selected by a `ShapeType` scalar parameter — `0` boot, `1` paw, `2` tyre, `3` track. A
profile sends it through `ScalarParameters`, which the subsystem pushes onto the batch's material
instance when the batch is created. A profile that sends nothing gets the default, which is the boot
shape.

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

## 12. The stats overlay

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

Add your own with `AddButton(Id, Label, Accent)` and `SetButtonLabel(Id, Label)` (§6.9). Built-in and
custom buttons fire the same `OnButtonClicked` delegate, so one Blueprint handler can drive the lot.

Layout is configurable: `StatsBoxOrigin`, `ButtonPanelRightMargin`, `ButtonPanelTop`, `ButtonWidth`,
`ButtonHeight`, `ButtonSpacing`, and the `BudgetToggleLow` / `BudgetToggleHigh` pair the BUDGET button
switches between.

---

## 13. Blueprint reference

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

### On the HUD

`Add Button`, `Set Button Label`, `Remove Button`, `Clear Buttons`, and the `On Button Clicked` event.

### On the settings

`Set Track Marks Enabled`, `Set Default Mark Budget`, `Set Max Retirements Per Tick`,
`Set Surface Trace Channel`, `Set Fallback Built In Profile`. These write to the in-memory defaults and
**never** touch the `.ini`, so an options-menu slider cannot corrupt the shipped configuration.

---

## 14. Project settings

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
| `Tick In Editor Worlds` | on | See §18 |

---

## 15. Console commands and variables

```
TrackMark.Test [Count] [Radius]   scatter marks in a disc in front of the local viewpoint
TrackMark.Budget [Count]          print, or set, the hard cap on live marks
TrackMark.Clear                   retire every live mark at once
TrackMark.Stats                   print the measured counters to the log
TrackMark.Enabled 0|1             console variable; refuses new marks, existing ones keep fading
```

`TrackMark.Test` reports how many marks actually landed. A mark over a hole or a wall is refused, not
faked — if 500 were requested and 480 placed, 20 found no ground or too steep a slope.

Defaults: `TrackMark.Test` uses 500 marks and a 900 cm radius when called with no arguments.

---

## 16. Demo content

The plugin ships a demo under `Content/TrackMark/` (mount path `/TrackMark/TrackMark/`). It is
self-contained and can be excluded from your cook if you do not want it.

| Path | What it is |
|---|---|
| `Maps/L_TrackMarkDemo` | An 80 × 80 m sand field with snow and mud patches, each carrying its own physical material |
| `Blueprints/BP_TrackMarkWalker` | Walks a circle laying marks. Four instances run at once, so the demo moves by itself |
| `Blueprints/BP_TrackMarkDemoHUD` | A Blueprint child of `ATrackMarkHUD` with three custom buttons on top of the four built-in ones |
| `Blueprints/BP_TrackMarkDemoViewer` | Camera pawn with a `CycleView` action |
| `Blueprints/BP_TrackMarkDemoGameMode` | Wires the HUD and the viewer together |
| `Materials/M_TrackMark` | **The master material.** This is the one asset the plugin itself depends on |
| `Materials/M_TrackMarkDemoGround` + `MI_…Sand/Snow/Mud` | Ground materials for the three surfaces |
| `Materials/M_TrackMarkDemoMarker` + four colour instances | Marker meshes that label the demo areas |
| `Profiles/DA_TrackMarkProfile_*` | Boot, Paw, Tyre, Track, SnowBoot, MudBoot — the asset profiles, which do carry `ShapeType` |
| `Physics/PM_TrackMarkSand / Snow / Mud` | The physical materials the surface mapping keys off |

Open the map, press Play, and the walkers start laying trails immediately. The overlay is on by default.

---

## 17. Performance notes

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

**Memory.** One live mark is a `{ int32, int32, float }` entry in the ring plus one instance in a batch.
A 2048-mark budget is a ring of about 24 KB plus the instance buffers, which the engine sizes for you.
Doubling the budget doubles both, linearly and predictably.

---

## 18. Limits and known caveats

**Built-in code profiles all draw the boot shape.** The four profiles created in C++ set no
`ScalarParameters`, so they never send `M_TrackMark` a `ShapeType` and the material falls back to its
default silhouette. Their sizes, lifetimes, opacities, strides, spacings and mirroring are all distinct
and correct — a Tyre code profile is 40 × 22 cm with a 22 cm stride and a 45 s life, which reads as a
tyre band — but the outline is a boot. The shipped **profile assets** send `ShapeType` and render the
right shape. If you want the code profiles to be shape-correct too, add
`ScalarParameters.Add(TEXT("ShapeType"), N)` per type in `UTrackMarkProfile::CreateBuiltIn`, or simply
point your components at the `DA_TrackMarkProfile_*` assets.

**HUD buttons and injected input.** The overlay's buttons are `AHUD` hit boxes. They respond to real
mouse clicks from a person. They do **not** respond to synthetic clicks injected at the OS level
(`SendInput` / `mouse_event`) — those reach the Slate layer of the window but not the Canvas hit-box
test. This is an engine behaviour, not a plugin bug, and it only matters for automated capture scripts.
Drive state changes through `UTrackMarkStatics` or the console when scripting.

**The decay clock.** The material derives a mark's age from the scene's `Time` input and the spawn time
written into custom data 0, which comes from `UWorld::GetTimeSeconds()`. These agree in Game and PIE.
The subsystem can also run in a plain **editor world** (`Tick In Editor Worlds`, on by default), where
the two clocks are the editor world's rather than a game world's; **this path has not been measured and
should be treated as untested.** If marks placed from an editor tool age oddly, switch the setting off
and place them in PIE.

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

**No terrain deformation, no audio, no gameplay.** TrackMark draws marks. It does not displace the
ground, does not play footstep sounds, does not animate anything, and implements no trail-reading
gameplay. See the README for the full "does not do" list.

---

## 19. Troubleshooting

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

**Every profile looks like a boot.**
You are using the built-in code profiles. Point the component at a `DA_TrackMarkProfile_*` asset
instead, or see §18.

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
*Game Only* after BeginPlay, it will take the cursor back. If you are clicking with a script rather than
a hand, see §18.

**Draw calls keep climbing.**
Each distinct profile in use is a batch. If something is creating profile assets at runtime, every one
is a new batch. Reuse profiles.

**Marks pop out at the screen edge.**
Instances sit far from their batch component's origin, so the component bounds need slack. Raise
*Batch Bounds Scale*.

---

## 20. Support

**Silvan Teufel**

- E-mail: <teufelsilvan@gmail.com>
- Documentation and issues: <https://github.com/SimulatedFlow/ue-plugin-TrackMark>

When reporting a problem, the output of `TrackMark.Stats` plus your engine version and platform is
usually enough to identify it.

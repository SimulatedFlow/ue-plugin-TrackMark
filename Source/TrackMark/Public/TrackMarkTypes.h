// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TrackMarkTypes.generated.h"

class AActor;
class UTrackMarkProfile;

namespace TrackMark
{
	/** Every batch component is created with exactly this many per-instance custom data floats. */
	static constexpr int32 NumCustomDataFloats = 4;

	/**
	 * Custom data layout. The material derives the age itself:
	 *
	 *   NormalisedAge = saturate((Time - CustomData0) * CustomData1)
	 *
	 * so a mark is written once when it is placed and never touched again until it is retired.
	 * There is no per-frame CPU loop over the live marks.
	 */
	/** World seconds at which the mark was placed. Compared against the material's Time input. */
	static constexpr int32 CustomDataIndex_SpawnTime = 0;
	/** 1 / lifetime in seconds. Multiplying beats dividing in a pixel shader, and 0 means "never fades". */
	static constexpr int32 CustomDataIndex_InvLifetime = 1;
	/** Peak opacity before decay, already scaled by the surface and the placement request. */
	static constexpr int32 CustomDataIndex_Opacity = 2;
	/**
	 * Variant index and side mirroring in one float, because four floats is the whole budget:
	 * magnitude - 1 is the variant index, the sign is the mirror flag (negative = mirrored).
	 * The material reads Variant = abs(x) - 1 and Mirror = sign(x).
	 */
	static constexpr int32 CustomDataIndex_VariantSigned = 3;

	/** Scale written to instances that must not rasterise. Not exactly zero so no transform math divides by it. */
	static constexpr float HiddenInstanceScale = 1.0e-4f;
}

/** The four profiles that exist in code, so marks appear before a single asset has been authored. */
UENUM(BlueprintType)
enum class ETrackMarkBuiltInProfile : uint8
{
	/** Humanoid boot print: two overlapping ellipses, mirrored per foot, medium lifetime. */
	Boot		UMETA(DisplayName = "Boot"),
	/** Animal paw: smaller, rounder, shorter lived. */
	Paw			UMETA(DisplayName = "Paw"),
	/** Wheeled vehicle: a long continuous band laid down at short intervals. */
	Tyre		UMETA(DisplayName = "Tyre"),
	/** Tracked vehicle: wide rectangular cleat pattern, longest lived. */
	Track		UMETA(DisplayName = "Track")
};

/** Which foot a mark belongs to. Drives the lateral offset and the mirror flag. */
UENUM(BlueprintType)
enum class ETrackMarkSide : uint8
{
	/** Offset to the left of the movement line, mirrored. */
	Left		UMETA(DisplayName = "Left"),
	/** Offset to the right of the movement line, not mirrored. */
	Right		UMETA(DisplayName = "Right"),
	/** Straight on the movement line, not mirrored. Right for tyres and single-track creatures. */
	Center		UMETA(DisplayName = "Center")
};

/** How a UTrackMarkComponent decides that it is time for another mark. */
UENUM(BlueprintType)
enum class ETrackMarkTriggerMode : uint8
{
	/**
	 * Nothing happens until something calls LeaveTrack(). Use this with an animation notify, where the
	 * foot plant is already authored. The component does not tick at all in this mode.
	 */
	Manual		UMETA(DisplayName = "Manual / Anim Notify"),
	/** The component ticks and drops a mark every stride length of travelled ground distance. */
	Distance	UMETA(DisplayName = "Distance"),
};

/** Why a placement request produced no mark. Returned so callers can tell "no ground" from "too steep". */
UENUM(BlueprintType)
enum class ETrackMarkResult : uint8
{
	/** A mark was placed. */
	Placed					UMETA(DisplayName = "Placed"),
	/** The system is switched off, project-wide or through the console variable. */
	Disabled				UMETA(DisplayName = "Disabled"),
	/** The downward trace found no ground within the trace distance. */
	NoGround				UMETA(DisplayName = "No Ground"),
	/** The surface is steeper than the profile's slope limit. */
	TooSteep				UMETA(DisplayName = "Too Steep"),
	/** No profile could be resolved, and no built-in fallback was configured. */
	NoProfile				UMETA(DisplayName = "No Profile"),
	/** The profile resolved but has no usable mesh, so there is nothing to instance. */
	NoMesh					UMETA(DisplayName = "No Mesh"),
	/** No instance slot could be obtained. Only happens when the batch component refuses an instance. */
	NoSlot					UMETA(DisplayName = "No Slot")
};

/**
 * What the subsystem is doing right now. Every number here is measured, not estimated.
 * Read it from Blueprint through UTrackMarkStatics::GetTrackMarkStats, or draw it with ATrackMarkHUD.
 */
USTRUCT(BlueprintType)
struct TRACKMARK_API FTrackMarkStats
{
	GENERATED_BODY()

	/** Marks currently alive, i.e. placed and not yet retired. Never exceeds the budget. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	int32 ActiveMarks = 0;

	/** The hard cap. Over it, the oldest mark is recycled instead of a new instance being allocated. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	int32 Budget = 0;

	/** Batch components in existence. Each one is one instanced static mesh, so one draw call. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	int32 Batches = 0;

	/** Batches that currently hold at least one live mark. The rest rasterise nothing. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	int32 ActiveBatches = 0;

	/** Instance slots allocated across all batches. This is the high-water mark and never shrinks. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	int32 InstanceSlots = 0;

	/** Allocated slots not currently holding a live mark. Free slots are what recycling hands back out. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	int32 FreeSlots = 0;

	/** Cumulative count of placements that reused an existing slot instead of allocating one. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	int32 RecycledSlots = 0;

	/** Cumulative count of placements that had to allocate a new instance. Flat after warm-up. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	int32 AllocatedSlots = 0;

	/** Surface traces performed during the previous frame. The only notable per-mark cost. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	int32 TracesLastFrame = 0;

	/** Marks placed during the previous frame. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	int32 PlacedLastFrame = 0;

	/** Marks retired by age during the previous frame. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	int32 RetiredLastFrame = 0;

	/** Requests rejected during the previous frame because the ground was steeper than the slope limit. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	int32 RejectedBySlopeLastFrame = 0;

	/** Wall-clock milliseconds the subsystem spent in its own tick last frame (ageing and bookkeeping). */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	float TickMilliseconds = 0.0f;

	/** Wall-clock milliseconds spent inside placement calls during the previous frame, traces included. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark")
	float PlacementMilliseconds = 0.0f;
};

/**
 * Everything one mark needs to know about itself. Filled in by the component or by Blueprint and handed
 * to UTrackMarkSubsystem::PlaceMark. Defaults on their own already produce a sensible boot print.
 */
USTRUCT(BlueprintType)
struct TRACKMARK_API FTrackMarkRequest
{
	GENERATED_BODY()

	/** World location of the foot. With tracing on this is the start of the downward trace, not the mark itself. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark")
	FVector Location = FVector::ZeroVector;

	/** Which way the mark points. Projected onto the surface plane, so it never has to be exact. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark")
	FVector Forward = FVector::ForwardVector;

	/** Profile to use. Null means: resolve from the surface's physical material, then from the default. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark")
	TObjectPtr<UTrackMarkProfile> Profile = nullptr;

	/**
	 * Let the physical material under the foot pick the profile, ahead of the one named above.
	 * Off, the profile above wins and the surface mapping is skipped entirely.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark")
	bool bAllowSurfaceProfile = true;

	/** Left, right or centred on the movement line. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark")
	ETrackMarkSide Side = ETrackMarkSide::Center;

	/** Trace down for the real surface point, normal and physical material. Off places the mark exactly at Location. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark")
	bool bTraceForSurface = true;

	/** How far above Location the trace starts, so a foot slightly inside the floor still finds it. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark", meta = (ClampMin = "0.0"))
	float TraceUpDistance = 40.0f;

	/** How far below Location the trace reaches before giving up. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark", meta = (ClampMin = "1.0"))
	float TraceDownDistance = 200.0f;

	/** Excluded from the trace, so a character never finds its own capsule. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark")
	TObjectPtr<AActor> IgnoreActor = nullptr;

	/** Multiplies the profile's opacity. Use it for a light step or a deep one. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float OpacityScale = 1.0f;

	/** Multiplies the profile's mark size. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark", meta = (ClampMin = "0.01", ClampMax = "16.0"))
	float SizeScale = 1.0f;

	/** Multiplies the profile's lifetime. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark", meta = (ClampMin = "0.0", ClampMax = "16.0"))
	float LifetimeScale = 1.0f;

	/**
	 * Sideways offset in centimetres from the movement line, overriding the profile's lateral spacing.
	 * Negative values mean "use the profile". Ignored for ETrackMarkSide::Center.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark")
	float LateralSpacingOverride = -1.0f;
};

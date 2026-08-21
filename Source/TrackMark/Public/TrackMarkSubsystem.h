// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TrackMarkTypes.h"
#include "TrackMarkSubsystem.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPhysicalMaterial;
class UStaticMesh;
class UTrackMarkProfile;

/**
 * One batch = one profile in use = one instanced static mesh component = one draw call.
 *
 * Instance slots are allocated once and then recycled forever. A slot is never removed from the
 * component, because removing an instance swaps the last one into the hole and would invalidate every
 * index above it - and this system hands out indices by the thousand.
 */
USTRUCT()
struct FTrackMarkBatch
{
	GENERATED_BODY()

	/** Profile this batch renders. Also the batch key: one profile in use is one batch is one draw call. */
	UPROPERTY(Transient)
	TObjectPtr<UTrackMarkProfile> Profile = nullptr;

	/** Mesh every instance in this batch uses. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** Material the batch's dynamic instance was created from. Null keeps the mesh's own material. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BaseMaterial = nullptr;

	/** Per-profile material instance carrying the decay curve, the tint and the variant count. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance = nullptr;

	/** The single component every mark of this profile lives in. */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Component = nullptr;

	/** Slot indices that hold no live mark and can be handed out again without allocating. */
	TArray<int32> FreeSlots;

	/** Reused every clear so even wiping 16k marks allocates nothing. */
	TArray<FTransform> ScratchTransforms;

	/** Instance slots ever allocated in this batch. Equals Component->GetInstanceCount(). */
	int32 NumSlots = 0;
};

/**
 * Owns every ground mark in a world: the instance pools, the budget, the ageing and the numbers.
 *
 * Placement is the only work that costs anything. A mark is written exactly once - one transform and
 * four custom data floats - and then the CPU forgets about it. Fading happens in the material, which
 * derives the mark's age from its spawn time and the scene clock, so ten thousand live marks cost zero
 * CPU per frame.
 *
 * The tick does two things: it retires marks whose lifetime has run out, and it publishes the stats.
 * Both are bounded.
 */
UCLASS()
class TRACKMARK_API UTrackMarkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override;

	/** Convenience getter. Returns null outside a world that supports the subsystem. */
	static UTrackMarkSubsystem* Get(const UObject* WorldContextObject);

	//~ Placement ----------------------------------------------------------------------------------------

	/**
	 * Place one mark. This is the whole hot path: an optional downward trace, a profile lookup, a slot,
	 * and two writes into the instanced component.
	 */
	ETrackMarkResult PlaceMark(const FTrackMarkRequest& Request);

	/** Blueprint-facing placement. See UTrackMarkStatics for the friendlier wrappers. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark", meta = (AutoCreateRefTerm = "Request"))
	ETrackMarkResult PlaceTrackMark(const FTrackMarkRequest& Request);

	/** Retire every live mark at once. One bulk write per batch, no allocation, no hitch. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark")
	void ClearAllMarks();

	//~ Budget -------------------------------------------------------------------------------------------

	/**
	 * Change the hard cap on live marks. Lowering it retires the oldest marks immediately; the instance
	 * slots stay allocated and are recycled from then on, so the draw call count does not move.
	 * This is the one call that allocates on purpose, because the ring buffer is resized.
	 */
	UFUNCTION(BlueprintCallable, Category = "TrackMark")
	void SetMarkBudget(int32 NewBudget);

	/** Current hard cap on live marks. */
	UFUNCTION(BlueprintPure, Category = "TrackMark")
	int32 GetMarkBudget() const { return MarkBudget; }

	//~ Profiles -----------------------------------------------------------------------------------------

	/** One of the four profiles that exist in code. Created on first use and kept for the world's lifetime. */
	UFUNCTION(BlueprintPure, Category = "TrackMark")
	UTrackMarkProfile* GetBuiltInProfile(ETrackMarkBuiltInProfile Type);

	/** Profile used when nothing else resolves. Never returns null once the settings are valid. */
	UFUNCTION(BlueprintPure, Category = "TrackMark")
	UTrackMarkProfile* GetDefaultProfile();

	/** Override the default profile for this world only, without touching the project settings. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark")
	void SetDefaultProfile(UTrackMarkProfile* NewDefault);

	/** Map a physical material to a profile for this world only. Pass a null profile to remove the mapping. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark")
	void SetProfileForSurface(UPhysicalMaterial* PhysicalMaterial, UTrackMarkProfile* Profile);

	/** Profile that a surface would resolve to, or null when it falls through to the default. */
	UFUNCTION(BlueprintPure, Category = "TrackMark")
	UTrackMarkProfile* GetProfileForSurface(UPhysicalMaterial* PhysicalMaterial) const;

	/**
	 * Force every placement to use this profile, whatever the surface or the caller asked for.
	 * This is what a "PROFILE: BOOT / PAW / TYRE / TRACK" button switches. Pass null to go back to normal.
	 */
	UFUNCTION(BlueprintCallable, Category = "TrackMark")
	void SetProfileOverride(UTrackMarkProfile* Profile);

	/** The forced profile, or null when surface resolution is in charge. */
	UFUNCTION(BlueprintPure, Category = "TrackMark")
	UTrackMarkProfile* GetProfileOverride() const { return ProfileOverride; }

	//~ State --------------------------------------------------------------------------------------------

	/**
	 * Turn placement on or off for this world. Off, new requests return Disabled; marks already on the
	 * ground keep fading and keep retiring, so nothing is stranded.
	 */
	UFUNCTION(BlueprintCallable, Category = "TrackMark")
	void SetMarksEnabled(bool bNewEnabled);

	/** True while this world accepts new marks. */
	UFUNCTION(BlueprintPure, Category = "TrackMark")
	bool AreMarksEnabled() const;

	/** Measured counters, refreshed once per tick. */
	UFUNCTION(BlueprintPure, Category = "TrackMark")
	const FTrackMarkStats& GetStats() const { return Stats; }

	/** Write the current numbers into the log, one line per counter. Backs the TrackMark.Stats command. */
	void LogStats() const;

	/**
	 * Scatter Count marks in a spiral around Origin, for the TrackMark.Test console command.
	 * Returns how many actually landed - a mark over a hole or a wall is refused, not faked.
	 */
	int32 SpawnTestMarks(int32 Count, const FVector& Origin, float Radius, UTrackMarkProfile* Profile);

private:
	/** One live mark. Deliberately small: the ring holds one of these per budgeted mark. */
	struct FTrackMarkEntry
	{
		/** Index into Batches. */
		int32 BatchIndex = INDEX_NONE;
		/** Instance slot inside that batch. */
		int32 SlotIndex = INDEX_NONE;
		/** World seconds at which this mark may be retired. FLT_MAX for a mark that never ages out. */
		float ExpireTime = 0.0f;
	};

	/** Re-read the project settings into the cached hot fields. Called on init and on every settings change. */
	void ApplySettings();

	/** Reacts to a runtime settings setter: re-reads everything and re-resolves the surface map. */
	void OnSettingsChanged();

	/** Find the batch for this profile, creating the component and the material instance on first use. */
	int32 FindOrCreateBatch(UTrackMarkProfile* Profile);

	/** Spawn the transient actor every batch component is attached to. */
	void EnsureBatchHolder();

	/** Build, configure and register a fresh instanced component for a batch. */
	UInstancedStaticMeshComponent* CreateBatchComponent(UStaticMesh* Mesh, UMaterialInterface* Material);

	/** Resolve a profile's mesh, falling back to the project default. */
	UStaticMesh* ResolveMesh(UTrackMarkProfile* Profile);

	/** Resolve a profile's material, falling back to the project default. Null means "keep the mesh's own". */
	UMaterialInterface* ResolveMaterial(UTrackMarkProfile* Profile);

	/** Take a slot in the given batch, recycling the world's oldest mark first if the budget is full. */
	bool AcquireSlot(int32 BatchIndex, int32& OutSlotIndex);

	/** Collapse an instance to nothing and hand its slot back to the batch. */
	void RetireEntry(const FTrackMarkEntry& Entry);

	/** Retire the oldest live mark. Returns false when nothing is live. */
	bool RetireOldest();

	/** Retire everything whose lifetime has run out, up to MaxRetirementsPerTick. */
	int32 RetireExpired(float WorldTimeSeconds);

	/** Resize the ring buffer, preserving age order. The only planned allocation after warm-up. */
	void ResizeRing(int32 NewCapacity);

	/** Read-only access to the oldest live entry. */
	const FTrackMarkEntry& PeekOldest() const { return Ring[RingStart]; }

	/** Rebuild the runtime physical-material map from the settings. */
	void RebuildSurfaceMap();

	/** Every batch created so far. Indices are stable; batches live as long as the world. */
	UPROPERTY(Transient)
	TArray<FTrackMarkBatch> Batches;

	/** Transient actor owning every batch component. Never saved. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> BatchHolder = nullptr;

	/** The four code profiles, created lazily and kept alive so the GC leaves them alone. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTrackMarkProfile>> BuiltInProfiles;

	/** Resolved default profile for this world. */
	UPROPERTY(Transient)
	TObjectPtr<UTrackMarkProfile> DefaultProfile = nullptr;

	/** Forced profile set through SetProfileOverride. */
	UPROPERTY(Transient)
	TObjectPtr<UTrackMarkProfile> ProfileOverride = nullptr;

	/** Runtime physical material to profile map, resolved from the soft pointers in the settings. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UPhysicalMaterial>, TObjectPtr<UTrackMarkProfile>> SurfaceProfiles;

	/** Lazily loaded project defaults. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> DefaultMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DefaultMaterial = nullptr;

	/** Live marks in age order. Capacity equals the budget; never grows during normal play. */
	TArray<FTrackMarkEntry> Ring;

	/** Index of the oldest live entry inside Ring. */
	int32 RingStart = 0;

	/** Number of live entries in Ring. */
	int32 RingCount = 0;

	/** Hard cap on live marks, seeded from the settings. */
	int32 MarkBudget = 2048;

	/** Cached hot settings, refreshed by ApplySettings. */
	int32 MaxRetirementsPerTick = 256;
	FVector2D ReferenceMeshSize = FVector2D(100.0f, 100.0f);
	float BatchBoundsScale = 2.0f;
	int32 InstanceStartCullDistance = 0;
	int32 InstanceEndCullDistance = 0;
	TEnumAsByte<ECollisionChannel> SurfaceTraceChannel = ECC_WorldStatic;
	bool bTraceComplex = false;
	bool bTickInEditorWorlds = true;

	/** Per-world master switch, seeded from the settings. */
	bool bMarksEnabled = true;

	/**
	 * Last value of the project-wide switch we acted on. A settings change overrides the per-world
	 * switch, but an unrelated settings change must not silently undo SetMarksEnabled.
	 */
	bool bSettingsEnabledSnapshot = true;

	/** Accumulators for the current frame; published into Stats at the start of the next tick. */
	int32 TracesThisFrame = 0;
	int32 PlacedThisFrame = 0;
	int32 RejectedBySlopeThisFrame = 0;
	double PlacementSecondsThisFrame = 0.0;

	/** Warn at most once per world about a missing mesh or material. */
	bool bLoggedMissingMesh = false;
	bool bLoggedMissingMaterial = false;

	/** Handle for the settings-changed delegate. */
	FDelegateHandle SettingsChangedHandle;

	/** Published through GetStats(). */
	FTrackMarkStats Stats;
};

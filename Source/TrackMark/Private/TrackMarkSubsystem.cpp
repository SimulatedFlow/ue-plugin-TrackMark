// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "TrackMarkSubsystem.h"

#include "CollisionQueryParams.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ScopeExit.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TrackMarkLog.h"
#include "TrackMarkProfile.h"
#include "TrackMarkSettings.h"

namespace TrackMarkPrivate
{
	/** Material parameter names the built-in M_TrackMark reads. Unknown names are ignored by the engine. */
	static const FName ParamName_FadeStart(TEXT("FadeStart"));
	static const FName ParamName_DecayExponent(TEXT("DecayExponent"));
	static const FName ParamName_VariantCount(TEXT("VariantCount"));
	static const FName ParamName_Tint(TEXT("Tint"));

	static TAutoConsoleVariable<int32> CVarEnabled(
		TEXT("TrackMark.Enabled"),
		1,
		TEXT("Enable TrackMark placement. 0 refuses new marks; marks already on the ground keep fading out."),
		ECVF_Default);

	/** Golden-angle spiral, so the test scatter fills a disc evenly instead of clumping into rings. */
	static constexpr float GoldenAngleDegrees = 137.50776f;

	/** Local helper: the subsystem for a world handed in by a console command. */
	static UTrackMarkSubsystem* GetSubsystem(UWorld* World)
	{
		return World ? World->GetSubsystem<UTrackMarkSubsystem>() : nullptr;
	}
}

//~ Lifetime ---------------------------------------------------------------------------------------------

bool UTrackMarkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* World = Cast<UWorld>(Outer);
	if (!World)
	{
		return false;
	}

	if (World->IsGameWorld())
	{
		return true;
	}

	// A plain editor world only gets a subsystem when the project asked for it, because it spawns a
	// transient holder actor and that is not something to do behind a level designer's back.
	return World->WorldType == EWorldType::Editor && UTrackMarkSettings::Get().bTickInEditorWorlds;
}

bool UTrackMarkSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::Editor;
}

void UTrackMarkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UTrackMarkSettings& Settings = UTrackMarkSettings::Get();

	bMarksEnabled = Settings.bEnableTrackMarks;
	bSettingsEnabledSnapshot = Settings.bEnableTrackMarks;
	MarkBudget = FMath::Max(1, Settings.MarkBudget);

	ApplySettings();
	RebuildSurfaceMap();
	ResizeRing(MarkBudget);

	Stats = FTrackMarkStats();
	Stats.Budget = MarkBudget;

	SettingsChangedHandle = UTrackMarkSettings::OnSettingsChanged().AddUObject(this, &UTrackMarkSubsystem::OnSettingsChanged);

	UE_LOG(LogTrackMark, Verbose, TEXT("TrackMark subsystem initialised (enabled=%d, budget=%d)."), bMarksEnabled ? 1 : 0, MarkBudget);
}

void UTrackMarkSubsystem::Deinitialize()
{
	if (SettingsChangedHandle.IsValid())
	{
		UTrackMarkSettings::OnSettingsChanged().Remove(SettingsChangedHandle);
		SettingsChangedHandle.Reset();
	}

	for (FTrackMarkBatch& Batch : Batches)
	{
		if (Batch.Component)
		{
			Batch.Component->DestroyComponent();
			Batch.Component = nullptr;
		}
	}
	Batches.Empty();

	if (IsValid(BatchHolder))
	{
		BatchHolder->Destroy();
	}
	BatchHolder = nullptr;

	Ring.Empty();
	RingStart = 0;
	RingCount = 0;

	BuiltInProfiles.Empty();
	SurfaceProfiles.Empty();
	DefaultProfile = nullptr;
	ProfileOverride = nullptr;
	DefaultMesh = nullptr;
	DefaultMaterial = nullptr;

	Super::Deinitialize();
}

TStatId UTrackMarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTrackMarkSubsystem, STATGROUP_Tickables);
}

bool UTrackMarkSubsystem::IsTickableInEditor() const
{
	return bTickInEditorWorlds;
}

UTrackMarkSubsystem* UTrackMarkSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UTrackMarkSubsystem>() : nullptr;
}

void UTrackMarkSubsystem::OnSettingsChanged()
{
	ApplySettings();
	RebuildSurfaceMap();

	// A changed project switch overrides the per-world one; an unrelated settings change does not.
	const UTrackMarkSettings& Settings = UTrackMarkSettings::Get();
	if (Settings.bEnableTrackMarks != bSettingsEnabledSnapshot)
	{
		bSettingsEnabledSnapshot = Settings.bEnableTrackMarks;
		bMarksEnabled = Settings.bEnableTrackMarks;
	}
}

void UTrackMarkSubsystem::ApplySettings()
{
	const UTrackMarkSettings& Settings = UTrackMarkSettings::Get();

	MaxRetirementsPerTick = FMath::Max(1, Settings.MaxRetirementsPerTick);
	ReferenceMeshSize = FVector2D(
		FMath::Max(0.01f, static_cast<float>(Settings.ReferenceMeshSize.X)),
		FMath::Max(0.01f, static_cast<float>(Settings.ReferenceMeshSize.Y)));
	BatchBoundsScale = FMath::Max(1.0f, Settings.BatchBoundsScale);
	InstanceStartCullDistance = FMath::Max(0, Settings.InstanceStartCullDistance);
	InstanceEndCullDistance = FMath::Max(0, Settings.InstanceEndCullDistance);
	SurfaceTraceChannel = Settings.SurfaceTraceChannel;
	bTraceComplex = Settings.bTraceComplex;
	bTickInEditorWorlds = Settings.bTickInEditorWorlds;
}

void UTrackMarkSubsystem::RebuildSurfaceMap()
{
	SurfaceProfiles.Reset();

	const UTrackMarkSettings& Settings = UTrackMarkSettings::Get();
	for (const TPair<TSoftObjectPtr<UPhysicalMaterial>, TSoftObjectPtr<UTrackMarkProfile>>& Pair : Settings.SurfaceProfiles)
	{
		UPhysicalMaterial* PhysicalMaterial = Pair.Key.LoadSynchronous();
		UTrackMarkProfile* Profile = Pair.Value.LoadSynchronous();
		if (PhysicalMaterial && Profile)
		{
			SurfaceProfiles.Add(PhysicalMaterial, Profile);
		}
	}
}

//~ Tick -------------------------------------------------------------------------------------------------

void UTrackMarkSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const double TickStartSeconds = FPlatformTime::Seconds();

	// Placement happens during actor ticks, which can run either side of this one. Publishing last
	// frame's accumulators instead of this frame's keeps the numbers honest whatever the tick order.
	Stats.TracesLastFrame = TracesThisFrame;
	Stats.PlacedLastFrame = PlacedThisFrame;
	Stats.RejectedBySlopeLastFrame = RejectedBySlopeThisFrame;
	Stats.PlacementMilliseconds = static_cast<float>(PlacementSecondsThisFrame * 1000.0);

	TracesThisFrame = 0;
	PlacedThisFrame = 0;
	RejectedBySlopeThisFrame = 0;
	PlacementSecondsThisFrame = 0.0;

	const UWorld* World = GetWorld();
	const float WorldTimeSeconds = World ? World->GetTimeSeconds() : 0.0f;

	Stats.RetiredLastFrame = RetireExpired(WorldTimeSeconds);

	int32 TotalSlots = 0;
	int32 TotalFreeSlots = 0;
	int32 ActiveBatches = 0;
	for (const FTrackMarkBatch& Batch : Batches)
	{
		TotalSlots += Batch.NumSlots;
		TotalFreeSlots += Batch.FreeSlots.Num();
		if (Batch.NumSlots > Batch.FreeSlots.Num())
		{
			++ActiveBatches;
		}
	}

	Stats.ActiveMarks = RingCount;
	Stats.Budget = MarkBudget;
	Stats.Batches = Batches.Num();
	Stats.ActiveBatches = ActiveBatches;
	Stats.InstanceSlots = TotalSlots;
	Stats.FreeSlots = TotalFreeSlots;
	Stats.TickMilliseconds = static_cast<float>((FPlatformTime::Seconds() - TickStartSeconds) * 1000.0);
}

//~ Placement --------------------------------------------------------------------------------------------

ETrackMarkResult UTrackMarkSubsystem::PlaceTrackMark(const FTrackMarkRequest& Request)
{
	return PlaceMark(Request);
}

ETrackMarkResult UTrackMarkSubsystem::PlaceMark(const FTrackMarkRequest& Request)
{
	if (!AreMarksEnabled())
	{
		return ETrackMarkResult::Disabled;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return ETrackMarkResult::Disabled;
	}

	const double PlacementStartSeconds = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		PlacementSecondsThisFrame += FPlatformTime::Seconds() - PlacementStartSeconds;
	};

	//~ 1. Find the surface. This is the only notable per-mark cost, and it is counted.
	FVector SurfaceLocation = Request.Location;
	FVector SurfaceNormal = FVector::UpVector;
	UPhysicalMaterial* SurfaceMaterial = nullptr;

	if (Request.bTraceForSurface)
	{
		const FVector TraceStart = Request.Location + FVector::UpVector * FMath::Max(0.0f, Request.TraceUpDistance);
		const FVector TraceEnd = Request.Location - FVector::UpVector * FMath::Max(1.0f, Request.TraceDownDistance);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TrackMarkSurface), bTraceComplex);
		QueryParams.bReturnPhysicalMaterial = true;
		if (Request.IgnoreActor)
		{
			QueryParams.AddIgnoredActor(Request.IgnoreActor);
		}

		FHitResult Hit;
		++TracesThisFrame;
		if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, SurfaceTraceChannel, QueryParams))
		{
			return ETrackMarkResult::NoGround;
		}

		SurfaceLocation = Hit.ImpactPoint;
		SurfaceNormal = Hit.ImpactNormal;
		SurfaceMaterial = Hit.PhysMaterial.Get();
	}

	//~ 2. Resolve the profile. A surface with no physical material is not an error, it just falls through.
	UTrackMarkProfile* Profile = ProfileOverride;
	if (!Profile && Request.bAllowSurfaceProfile && SurfaceMaterial)
	{
		Profile = SurfaceProfiles.FindRef(SurfaceMaterial);
	}
	if (!Profile)
	{
		Profile = Request.Profile;
	}
	if (!Profile)
	{
		Profile = GetDefaultProfile();
	}
	if (!Profile)
	{
		return ETrackMarkResult::NoProfile;
	}

	//~ 3. Refuse ground that is too steep. A footprint plastered on a wall is worse than no footprint.
	const float SurfaceSlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(SurfaceNormal.Z), -1.0f, 1.0f)));
	if (SurfaceSlopeDegrees > Profile->MaxSlopeAngle)
	{
		++RejectedBySlopeThisFrame;
		return ETrackMarkResult::TooSteep;
	}

	//~ 4. Get a slot. Over budget this recycles the world's oldest mark instead of allocating.
	const int32 BatchIndex = FindOrCreateBatch(Profile);
	if (!Batches.IsValidIndex(BatchIndex))
	{
		return ETrackMarkResult::NoMesh;
	}

	int32 SlotIndex = INDEX_NONE;
	if (!AcquireSlot(BatchIndex, SlotIndex))
	{
		return ETrackMarkResult::NoSlot;
	}

	//~ 5. Build the transform: up along the surface normal, forward along the direction of travel.
	FVector Up = Profile->bAlignToSurfaceNormal
		? SurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector)
		: FVector::UpVector;

	FVector Forward = Request.Forward - Up * (Request.Forward | Up);
	if (Forward.IsNearlyZero())
	{
		// The request pointed straight at the surface. Any tangent will do; pick a stable one.
		Forward = FVector::ForwardVector - Up * (FVector::ForwardVector | Up);
		if (Forward.IsNearlyZero())
		{
			Forward = FVector::RightVector - Up * (FVector::RightVector | Up);
		}
	}
	Forward = Forward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

	if (Profile->YawJitterDegrees > 0.0f)
	{
		Forward = Forward.RotateAngleAxis(FMath::FRandRange(-Profile->YawJitterDegrees, Profile->YawJitterDegrees), Up);
	}

	const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);

	const float LateralSpacing = Request.LateralSpacingOverride >= 0.0f ? Request.LateralSpacingOverride : Profile->LateralSpacing;
	const float SideSign = Request.Side == ETrackMarkSide::Left ? -1.0f : (Request.Side == ETrackMarkSide::Right ? 1.0f : 0.0f);

	FVector MarkLocation = SurfaceLocation
		+ Right * (LateralSpacing * 0.5f * SideSign)
		+ Up * Profile->SurfaceOffset
		+ Forward * Profile->LocalOffset.X
		+ Right * Profile->LocalOffset.Y
		+ Up * Profile->LocalOffset.Z;

	const float ScaleAlong = static_cast<float>(Profile->MarkSize.X * Request.SizeScale / ReferenceMeshSize.X);
	const float ScaleAcross = static_cast<float>(Profile->MarkSize.Y * Request.SizeScale / ReferenceMeshSize.Y);

	const FTransform MarkTransform(
		FRotationMatrix::MakeFromZX(Up, Forward).ToQuat(),
		MarkLocation,
		FVector(ScaleAlong, ScaleAcross, 1.0f));

	//~ 6. Write the mark exactly once. From here on the CPU never touches it again.
	const float NowSeconds = World->GetTimeSeconds();
	const float LifetimeSeconds = Profile->RollLifetime() * FMath::Max(0.0f, Request.LifetimeScale);
	const float InverseLifetime = Profile->ComputeInverseLifetime(LifetimeSeconds);
	const float Opacity = FMath::Max(0.0f, Profile->Opacity * Request.OpacityScale);

	const int32 VariantCount = FMath::Max(1, Profile->NumVariants);
	const int32 Variant = VariantCount > 1 ? FMath::RandHelper(VariantCount) : 0;
	const bool bMirrored = Profile->bMirrorLeftSide && Request.Side == ETrackMarkSide::Left;
	const float VariantSigned = static_cast<float>(Variant + 1) * (bMirrored ? -1.0f : 1.0f);

	float CustomData[TrackMark::NumCustomDataFloats];
	CustomData[TrackMark::CustomDataIndex_SpawnTime] = NowSeconds;
	CustomData[TrackMark::CustomDataIndex_InvLifetime] = InverseLifetime;
	CustomData[TrackMark::CustomDataIndex_Opacity] = Opacity;
	CustomData[TrackMark::CustomDataIndex_VariantSigned] = VariantSigned;

	FTrackMarkBatch& Batch = Batches[BatchIndex];
	if (!IsValid(Batch.Component))
	{
		return ETrackMarkResult::NoMesh;
	}

	// bMarkRenderStateDirty stays false on purpose: the instance data manager tracks the change and
	// streams it to the GPU scene. Recreating the render state would cost the whole batch every step.
	Batch.Component->UpdateInstanceTransform(SlotIndex, MarkTransform, /*bWorldSpace*/ true, /*bMarkRenderStateDirty*/ false, /*bTeleport*/ true);
	Batch.Component->SetCustomData(SlotIndex, MakeArrayView(CustomData), /*bMarkRenderStateDirty*/ false);

	//~ 7. Remember it, in age order, in the fixed-size ring.
	FTrackMarkEntry Entry;
	Entry.BatchIndex = BatchIndex;
	Entry.SlotIndex = SlotIndex;
	Entry.ExpireTime = LifetimeSeconds > 0.0f ? NowSeconds + LifetimeSeconds : TNumericLimits<float>::Max();

	const int32 WriteIndex = (RingStart + RingCount) % Ring.Num();
	Ring[WriteIndex] = Entry;
	++RingCount;

	++PlacedThisFrame;
	return ETrackMarkResult::Placed;
}

int32 UTrackMarkSubsystem::SpawnTestMarks(int32 Count, const FVector& Origin, float Radius, UTrackMarkProfile* Profile)
{
	const int32 RequestedCount = FMath::Max(0, Count);
	const float SpiralRadius = FMath::Max(1.0f, Radius);

	int32 NumPlaced = 0;
	for (int32 Index = 0; Index < RequestedCount; ++Index)
	{
		// Sunflower distribution: sqrt on the radius keeps the density even out to the edge.
		const float Fraction = (static_cast<float>(Index) + 0.5f) / static_cast<float>(FMath::Max(1, RequestedCount));
		const float Distance = SpiralRadius * FMath::Sqrt(Fraction);
		const float AngleRadians = FMath::DegreesToRadians(TrackMarkPrivate::GoldenAngleDegrees * static_cast<float>(Index));

		FTrackMarkRequest Request;
		Request.Location = Origin + FVector(FMath::Cos(AngleRadians) * Distance, FMath::Sin(AngleRadians) * Distance, 0.0f);
		Request.Forward = FVector(-FMath::Sin(AngleRadians), FMath::Cos(AngleRadians), 0.0f);
		Request.Profile = Profile;
		Request.Side = (Index % 2) == 0 ? ETrackMarkSide::Left : ETrackMarkSide::Right;
		// Generous trace bounds: the command is aimed at a viewpoint, not at a known floor height.
		Request.TraceUpDistance = 500.0f;
		Request.TraceDownDistance = 5000.0f;

		if (PlaceMark(Request) == ETrackMarkResult::Placed)
		{
			++NumPlaced;
		}
	}

	return NumPlaced;
}

//~ Slots and retirement ---------------------------------------------------------------------------------

bool UTrackMarkSubsystem::AcquireSlot(int32 BatchIndex, int32& OutSlotIndex)
{
	OutSlotIndex = INDEX_NONE;

	if (!Batches.IsValidIndex(BatchIndex) || Ring.Num() <= 0)
	{
		return false;
	}

	// Over the cap the oldest mark in the world dies so this one can live. That is the whole budget rule.
	while (RingCount >= Ring.Num())
	{
		if (!RetireOldest())
		{
			return false;
		}
	}

	FTrackMarkBatch& Batch = Batches[BatchIndex];
	if (!IsValid(Batch.Component))
	{
		return false;
	}

	if (Batch.FreeSlots.Num() > 0)
	{
		OutSlotIndex = Batch.FreeSlots.Pop(EAllowShrinking::No);
		++Stats.RecycledSlots;
		return true;
	}

	// First time this batch has needed this many marks. This is the warm-up, and only the warm-up.
	const FTransform InitialTransform(FQuat::Identity, FVector::ZeroVector, FVector(TrackMark::HiddenInstanceScale));
	const int32 NewSlot = Batch.Component->AddInstance(InitialTransform, /*bWorldSpace*/ false);
	if (NewSlot == INDEX_NONE)
	{
		return false;
	}

	Batch.NumSlots = Batch.Component->GetInstanceCount();
	++Stats.AllocatedSlots;
	OutSlotIndex = NewSlot;
	return true;
}

void UTrackMarkSubsystem::RetireEntry(const FTrackMarkEntry& Entry)
{
	if (!Batches.IsValidIndex(Entry.BatchIndex))
	{
		return;
	}

	FTrackMarkBatch& Batch = Batches[Entry.BatchIndex];
	if (!IsValid(Batch.Component) || Entry.SlotIndex < 0 || Entry.SlotIndex >= Batch.NumSlots)
	{
		return;
	}

	// Collapse rather than remove: removing an instance swaps the last one into the hole and would
	// invalidate every slot index above it, and this system hands out indices by the thousand.
	const FTransform HiddenTransform(FQuat::Identity, FVector::ZeroVector, FVector(TrackMark::HiddenInstanceScale));
	Batch.Component->UpdateInstanceTransform(Entry.SlotIndex, HiddenTransform, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ false, /*bTeleport*/ true);

	Batch.FreeSlots.Add(Entry.SlotIndex);
}

bool UTrackMarkSubsystem::RetireOldest()
{
	if (RingCount <= 0 || Ring.Num() <= 0)
	{
		return false;
	}

	RetireEntry(Ring[RingStart]);
	RingStart = (RingStart + 1) % Ring.Num();
	--RingCount;
	return true;
}

int32 UTrackMarkSubsystem::RetireExpired(float WorldTimeSeconds)
{
	int32 NumRetired = 0;

	// The ring is in placement order, and lifetimes only vary by the profile's jitter, so walking from
	// the oldest end and stopping at the first survivor is both correct enough and bounded.
	while (RingCount > 0 && NumRetired < MaxRetirementsPerTick)
	{
		if (PeekOldest().ExpireTime > WorldTimeSeconds)
		{
			break;
		}

		RetireOldest();
		++NumRetired;
	}

	return NumRetired;
}

void UTrackMarkSubsystem::ResizeRing(int32 NewCapacity)
{
	NewCapacity = FMath::Max(1, NewCapacity);

	while (RingCount > NewCapacity)
	{
		if (!RetireOldest())
		{
			break;
		}
	}

	TArray<FTrackMarkEntry> NewRing;
	NewRing.SetNum(NewCapacity);

	if (Ring.Num() > 0)
	{
		const int32 NumToCopy = FMath::Min(RingCount, NewCapacity);
		for (int32 Index = 0; Index < NumToCopy; ++Index)
		{
			NewRing[Index] = Ring[(RingStart + Index) % Ring.Num()];
		}
		RingCount = NumToCopy;
	}
	else
	{
		RingCount = 0;
	}

	Ring = MoveTemp(NewRing);
	RingStart = 0;
}

void UTrackMarkSubsystem::SetMarkBudget(int32 NewBudget)
{
	const int32 Clamped = FMath::Max(1, NewBudget);
	if (Clamped == MarkBudget && Ring.Num() == Clamped)
	{
		return;
	}

	MarkBudget = Clamped;
	ResizeRing(MarkBudget);
	Stats.Budget = MarkBudget;
	Stats.ActiveMarks = RingCount;

	UE_LOG(LogTrackMark, Log, TEXT("Budget set to %d (%d marks live, %d instance slots allocated)."),
		MarkBudget, RingCount, Stats.InstanceSlots);
}

void UTrackMarkSubsystem::ClearAllMarks()
{
	for (FTrackMarkBatch& Batch : Batches)
	{
		if (!IsValid(Batch.Component) || Batch.NumSlots <= 0)
		{
			continue;
		}

		// One bulk write per batch instead of one call per mark: clearing 16k marks is a single update.
		const FTransform HiddenTransform(FQuat::Identity, FVector::ZeroVector, FVector(TrackMark::HiddenInstanceScale));
		Batch.ScratchTransforms.SetNum(Batch.NumSlots, EAllowShrinking::No);
		for (int32 Index = 0; Index < Batch.NumSlots; ++Index)
		{
			Batch.ScratchTransforms[Index] = HiddenTransform;
		}

		Batch.Component->BatchUpdateInstancesTransforms(
			0, Batch.ScratchTransforms, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ false, /*bTeleport*/ true);

		Batch.FreeSlots.Reset(Batch.NumSlots);
		for (int32 Index = Batch.NumSlots - 1; Index >= 0; --Index)
		{
			Batch.FreeSlots.Add(Index);
		}
	}

	RingStart = 0;
	RingCount = 0;
	Stats.ActiveMarks = 0;
}

//~ Batches ----------------------------------------------------------------------------------------------

int32 UTrackMarkSubsystem::FindOrCreateBatch(UTrackMarkProfile* Profile)
{
	if (!Profile)
	{
		return INDEX_NONE;
	}

	// Batch counts are tiny in practice - one per profile actually in use - so a linear scan beats a map.
	for (int32 BatchIndex = 0; BatchIndex < Batches.Num(); ++BatchIndex)
	{
		if (Batches[BatchIndex].Profile == Profile && IsValid(Batches[BatchIndex].Component))
		{
			return BatchIndex;
		}
	}

	UStaticMesh* Mesh = ResolveMesh(Profile);
	if (!Mesh)
	{
		return INDEX_NONE;
	}

	UMaterialInterface* BaseMaterial = ResolveMaterial(Profile);

	UInstancedStaticMeshComponent* Component = CreateBatchComponent(Mesh, nullptr);
	if (!Component)
	{
		return INDEX_NONE;
	}

	// The profile's look lives in its own material instance, which is why two profiles sharing a mesh are
	// still two batches - and still one draw call each.
	UMaterialInstanceDynamic* MaterialInstance = nullptr;
	if (BaseMaterial)
	{
		MaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, Component);
		if (MaterialInstance)
		{
			MaterialInstance->SetScalarParameterValue(TrackMarkPrivate::ParamName_FadeStart, FMath::Clamp(Profile->FadeStartFraction, 0.0f, 1.0f));
			MaterialInstance->SetScalarParameterValue(TrackMarkPrivate::ParamName_DecayExponent, FMath::Max(0.05f, Profile->DecayExponent));
			MaterialInstance->SetScalarParameterValue(TrackMarkPrivate::ParamName_VariantCount, static_cast<float>(FMath::Max(1, Profile->NumVariants)));
			MaterialInstance->SetVectorParameterValue(TrackMarkPrivate::ParamName_Tint, Profile->Tint);

			for (const TPair<FName, float>& Pair : Profile->ScalarParameters)
			{
				MaterialInstance->SetScalarParameterValue(Pair.Key, Pair.Value);
			}
			for (const TPair<FName, FLinearColor>& Pair : Profile->VectorParameters)
			{
				MaterialInstance->SetVectorParameterValue(Pair.Key, Pair.Value);
			}

			Component->SetMaterial(0, MaterialInstance);
		}
		else
		{
			Component->SetMaterial(0, BaseMaterial);
		}
	}

	FTrackMarkBatch NewBatch;
	NewBatch.Profile = Profile;
	NewBatch.Mesh = Mesh;
	NewBatch.BaseMaterial = BaseMaterial;
	NewBatch.MaterialInstance = MaterialInstance;
	NewBatch.Component = Component;
	NewBatch.NumSlots = 0;

	const int32 NewIndex = Batches.Add(MoveTemp(NewBatch));
	UE_LOG(LogTrackMark, Log, TEXT("Created batch %d for profile '%s' on mesh '%s' (one draw call)."),
		NewIndex, *GetNameSafe(Profile), *GetNameSafe(Mesh));

	return NewIndex;
}

void UTrackMarkSubsystem::EnsureBatchHolder()
{
	if (IsValid(BatchHolder))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
#if WITH_EDITOR
	// It exists to own components; putting it in the outliner would only invite someone to delete it.
	SpawnParams.bHideFromSceneOutliner = true;
#endif

	BatchHolder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!BatchHolder)
	{
		UE_LOG(LogTrackMark, Error, TEXT("Failed to spawn the TrackMark batch holder; no marks will be rendered."));
		return;
	}

	USceneComponent* Root = NewObject<USceneComponent>(BatchHolder, TEXT("TrackMarkBatchRoot"), RF_Transient);
	BatchHolder->SetRootComponent(Root);
	Root->RegisterComponent();

	BatchHolder->SetActorEnableCollision(false);
	BatchHolder->SetCanBeDamaged(false);
}

UInstancedStaticMeshComponent* UTrackMarkSubsystem::CreateBatchComponent(UStaticMesh* Mesh, UMaterialInterface* Material)
{
	EnsureBatchHolder();
	if (!IsValid(BatchHolder) || !Mesh)
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(BatchHolder, NAME_None, RF_Transient);
	if (!Component)
	{
		return nullptr;
	}

	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}

	// A flat mark on the ground has no business casting or occluding anything, and lighting work on it
	// is pure waste. Everything below is switched off deliberately, not by accident.
	Component->SetCastShadow(false);
	Component->bCastDynamicShadow = false;
	Component->bCastStaticShadow = false;
	Component->bCastVolumetricTranslucentShadow = false;
	Component->bCastContactShadow = false;
	Component->bAffectDynamicIndirectLighting = false;
	Component->bAffectDistanceFieldLighting = false;
	Component->bUseAsOccluder = false;
	Component->SetReceivesDecals(false);

	// No collision, no navigation, no overlap bookkeeping: these are render-only proxies. Navigation in
	// particular matters - thousands of instances marked navigation-relevant would rebuild the navmesh.
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);

	// Instances are spread across the level far from the holder at the origin, so the component bounds
	// need slack or a whole trail pops out at the edge of the screen.
	Component->BoundsScale = BatchBoundsScale;
	Component->SetCullDistances(InstanceStartCullDistance, InstanceEndCullDistance);

	Component->SetNumCustomDataFloats(TrackMark::NumCustomDataFloats);

	Component->SetupAttachment(BatchHolder->GetRootComponent());
	Component->RegisterComponent();
	BatchHolder->AddInstanceComponent(Component);

	return Component;
}

UStaticMesh* UTrackMarkSubsystem::ResolveMesh(UTrackMarkProfile* Profile)
{
	if (Profile && !Profile->Mesh.IsNull())
	{
		if (UStaticMesh* ProfileMesh = Profile->Mesh.LoadSynchronous())
		{
			return ProfileMesh;
		}
	}

	if (DefaultMesh)
	{
		return DefaultMesh;
	}

	const UTrackMarkSettings& Settings = UTrackMarkSettings::Get();
	if (!Settings.DefaultMarkMesh.IsNull())
	{
		DefaultMesh = Cast<UStaticMesh>(Settings.DefaultMarkMesh.TryLoad());
	}

	if (!DefaultMesh && !bLoggedMissingMesh)
	{
		bLoggedMissingMesh = true;
		UE_LOG(LogTrackMark, Error,
			TEXT("No mark mesh available. Set a Mesh on the profile, or a valid Default Mark Mesh under ")
			TEXT("Project Settings > Plugins > TrackMark."));
	}

	return DefaultMesh;
}

UMaterialInterface* UTrackMarkSubsystem::ResolveMaterial(UTrackMarkProfile* Profile)
{
	if (Profile && !Profile->Material.IsNull())
	{
		if (UMaterialInterface* ProfileMaterial = Profile->Material.LoadSynchronous())
		{
			return ProfileMaterial;
		}
	}

	if (DefaultMaterial)
	{
		return DefaultMaterial;
	}

	const UTrackMarkSettings& Settings = UTrackMarkSettings::Get();
	if (!Settings.DefaultMarkMaterial.IsNull())
	{
		DefaultMaterial = Cast<UMaterialInterface>(Settings.DefaultMarkMaterial.TryLoad());
	}

	if (!DefaultMaterial && !bLoggedMissingMaterial)
	{
		bLoggedMissingMaterial = true;
		UE_LOG(LogTrackMark, Warning,
			TEXT("Default mark material '%s' could not be loaded; batches fall back to the mesh's own material. ")
			TEXT("Geometry and placement still work, but the marks will not read the custom data and will not fade."),
			*Settings.DefaultMarkMaterial.ToString());
	}

	return DefaultMaterial;
}

//~ Profiles ---------------------------------------------------------------------------------------------

UTrackMarkProfile* UTrackMarkSubsystem::GetBuiltInProfile(ETrackMarkBuiltInProfile Type)
{
	constexpr int32 NumBuiltIns = static_cast<int32>(ETrackMarkBuiltInProfile::Track) + 1;
	const int32 Index = FMath::Clamp(static_cast<int32>(Type), 0, NumBuiltIns - 1);

	if (BuiltInProfiles.Num() != NumBuiltIns)
	{
		BuiltInProfiles.SetNum(NumBuiltIns);
	}

	if (!BuiltInProfiles[Index])
	{
		BuiltInProfiles[Index] = UTrackMarkProfile::CreateBuiltIn(static_cast<ETrackMarkBuiltInProfile>(Index), this);
	}

	return BuiltInProfiles[Index];
}

UTrackMarkProfile* UTrackMarkSubsystem::GetDefaultProfile()
{
	if (DefaultProfile)
	{
		return DefaultProfile;
	}

	const UTrackMarkSettings& Settings = UTrackMarkSettings::Get();
	if (!Settings.DefaultProfile.IsNull())
	{
		DefaultProfile = Settings.DefaultProfile.LoadSynchronous();
	}

	if (!DefaultProfile)
	{
		// This is the reason a fresh install already leaves footprints: no asset, no configuration, no setup.
		DefaultProfile = GetBuiltInProfile(Settings.FallbackBuiltInProfile);
	}

	return DefaultProfile;
}

void UTrackMarkSubsystem::SetDefaultProfile(UTrackMarkProfile* NewDefault)
{
	DefaultProfile = NewDefault ? NewDefault : GetBuiltInProfile(UTrackMarkSettings::Get().FallbackBuiltInProfile);
}

void UTrackMarkSubsystem::SetProfileForSurface(UPhysicalMaterial* PhysicalMaterial, UTrackMarkProfile* Profile)
{
	if (!PhysicalMaterial)
	{
		return;
	}

	if (Profile)
	{
		SurfaceProfiles.Add(PhysicalMaterial, Profile);
	}
	else
	{
		SurfaceProfiles.Remove(PhysicalMaterial);
	}
}

UTrackMarkProfile* UTrackMarkSubsystem::GetProfileForSurface(UPhysicalMaterial* PhysicalMaterial) const
{
	if (!PhysicalMaterial)
	{
		return nullptr;
	}

	const TObjectPtr<UTrackMarkProfile>* Found = SurfaceProfiles.Find(PhysicalMaterial);
	return Found ? Found->Get() : nullptr;
}

void UTrackMarkSubsystem::SetProfileOverride(UTrackMarkProfile* Profile)
{
	ProfileOverride = Profile;
}

//~ State ------------------------------------------------------------------------------------------------

void UTrackMarkSubsystem::SetMarksEnabled(bool bNewEnabled)
{
	bMarksEnabled = bNewEnabled;
}

bool UTrackMarkSubsystem::AreMarksEnabled() const
{
	return bMarksEnabled && TrackMarkPrivate::CVarEnabled.GetValueOnGameThread() != 0;
}

void UTrackMarkSubsystem::LogStats() const
{
	UE_LOG(LogTrackMark, Display, TEXT("--- TrackMark ---"));
	UE_LOG(LogTrackMark, Display, TEXT("Active marks      %d / %d"), Stats.ActiveMarks, Stats.Budget);
	UE_LOG(LogTrackMark, Display, TEXT("Batches           %d (%d active, one draw call each)"), Stats.Batches, Stats.ActiveBatches);
	UE_LOG(LogTrackMark, Display, TEXT("Instance slots    %d (%d free)"), Stats.InstanceSlots, Stats.FreeSlots);
	UE_LOG(LogTrackMark, Display, TEXT("Slots recycled    %d"), Stats.RecycledSlots);
	UE_LOG(LogTrackMark, Display, TEXT("Slots allocated   %d"), Stats.AllocatedSlots);
	UE_LOG(LogTrackMark, Display, TEXT("Traces last frame %d"), Stats.TracesLastFrame);
	UE_LOG(LogTrackMark, Display, TEXT("Placed / retired  %d / %d"), Stats.PlacedLastFrame, Stats.RetiredLastFrame);
	UE_LOG(LogTrackMark, Display, TEXT("Rejected (slope)  %d"), Stats.RejectedBySlopeLastFrame);
	UE_LOG(LogTrackMark, Display, TEXT("Subsystem tick    %.3f ms"), Stats.TickMilliseconds);
	UE_LOG(LogTrackMark, Display, TEXT("Placement         %.3f ms"), Stats.PlacementMilliseconds);
}

//~ Console commands -------------------------------------------------------------------------------------

namespace TrackMarkConsole
{
	/** Origin for TrackMark.Test: a point in front of the local viewpoint, at the viewpoint's height. */
	static FVector ResolveTestOrigin(UWorld* World)
	{
		if (!World)
		{
			return FVector::ZeroVector;
		}

		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			FVector ViewLocation = FVector::ZeroVector;
			FRotator ViewRotation = FRotator::ZeroRotator;
			PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

			FVector Ahead = ViewRotation.Vector();
			Ahead.Z = 0.0f;
			Ahead = Ahead.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

			return ViewLocation + Ahead * 700.0f;
		}

		return FVector::ZeroVector;
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdTest(
		TEXT("TrackMark.Test"),
		TEXT("TrackMark.Test [Count] [Radius] - scatter marks in a disc in front of the local viewpoint."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UTrackMarkSubsystem* Subsystem = TrackMarkPrivate::GetSubsystem(World);
			if (!Subsystem)
			{
				UE_LOG(LogTrackMark, Warning, TEXT("TrackMark.Test: no TrackMark subsystem in this world."));
				return;
			}

			const int32 Count = Args.Num() > 0 ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 65536) : 500;
			const float Radius = Args.Num() > 1 ? FMath::Max(1.0f, FCString::Atof(*Args[1])) : 900.0f;

			const int32 NumPlaced = Subsystem->SpawnTestMarks(Count, ResolveTestOrigin(World), Radius, nullptr);
			UE_LOG(LogTrackMark, Display, TEXT("TrackMark.Test: %d of %d marks placed (the rest found no ground or too steep a slope)."),
				NumPlaced, Count);
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdBudget(
		TEXT("TrackMark.Budget"),
		TEXT("TrackMark.Budget [Count] - print or set the hard cap on live marks."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UTrackMarkSubsystem* Subsystem = TrackMarkPrivate::GetSubsystem(World);
			if (!Subsystem)
			{
				UE_LOG(LogTrackMark, Warning, TEXT("TrackMark.Budget: no TrackMark subsystem in this world."));
				return;
			}

			if (Args.Num() > 0)
			{
				Subsystem->SetMarkBudget(FCString::Atoi(*Args[0]));
			}

			UE_LOG(LogTrackMark, Display, TEXT("TrackMark budget: %d (%d live)."),
				Subsystem->GetMarkBudget(), Subsystem->GetStats().ActiveMarks);
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdClear(
		TEXT("TrackMark.Clear"),
		TEXT("TrackMark.Clear - retire every live mark at once. Instance slots stay allocated for reuse."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			UTrackMarkSubsystem* Subsystem = TrackMarkPrivate::GetSubsystem(World);
			if (!Subsystem)
			{
				UE_LOG(LogTrackMark, Warning, TEXT("TrackMark.Clear: no TrackMark subsystem in this world."));
				return;
			}

			Subsystem->ClearAllMarks();
			UE_LOG(LogTrackMark, Display, TEXT("TrackMark.Clear: all marks retired."));
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdStats(
		TEXT("TrackMark.Stats"),
		TEXT("TrackMark.Stats - print the measured counters to the log."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const UTrackMarkSubsystem* Subsystem = TrackMarkPrivate::GetSubsystem(World);
			if (!Subsystem)
			{
				UE_LOG(LogTrackMark, Warning, TEXT("TrackMark.Stats: no TrackMark subsystem in this world."));
				return;
			}

			Subsystem->LogStats();
		}));
}

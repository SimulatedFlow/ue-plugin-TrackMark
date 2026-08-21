// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TrackMarkTypes.h"
#include "TrackMarkStatics.generated.h"

class AActor;
class UPhysicalMaterial;
class UTrackMarkProfile;
class UTrackMarkSubsystem;

/**
 * The complete Blueprint surface of TrackMark. Nothing in this plugin needs a line of C++ to use.
 *
 * Every function takes a world context, resolves the world subsystem itself and fails quietly when
 * there is none - calling these from a menu level does nothing rather than crashing.
 */
UCLASS()
class TRACKMARK_API UTrackMarkStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//~ Placement ----------------------------------------------------------------------------------------

	/**
	 * Drop one mark at a world location. The system traces down for the real surface point, its normal
	 * and its physical material, so passing a location roughly at foot height is enough.
	 */
	UFUNCTION(BlueprintCallable, Category = "TrackMark", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Profile,Side,IgnoreActor"))
	static ETrackMarkResult LeaveTrackMark(
		const UObject* WorldContextObject,
		FVector Location,
		FVector ForwardDirection,
		UTrackMarkProfile* Profile = nullptr,
		ETrackMarkSide Side = ETrackMarkSide::Center,
		AActor* IgnoreActor = nullptr);

	/** Drop one mark under an actor, facing the way it is travelling. The actor is excluded from the trace. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark", meta = (AdvancedDisplay = "Profile,Side"))
	static ETrackMarkResult LeaveTrackMarkForActor(
		AActor* Actor,
		UTrackMarkProfile* Profile = nullptr,
		ETrackMarkSide Side = ETrackMarkSide::Center);

	/** Full control: fill in the request struct yourself. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Request"))
	static ETrackMarkResult LeaveTrackMarkFromRequest(const UObject* WorldContextObject, const FTrackMarkRequest& Request);

	/** Wipe every live mark in the world. One bulk write per batch; no hitch, no reallocation. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static void ClearAllTrackMarks(const UObject* WorldContextObject);

	//~ Budget and state ---------------------------------------------------------------------------------

	/** Change the hard cap on live marks. Lowering it retires the oldest immediately, without freeing slots. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static void SetTrackMarkBudget(const UObject* WorldContextObject, int32 NewBudget);

	/** Current hard cap on live marks, or 0 when there is no subsystem. */
	UFUNCTION(BlueprintPure, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static int32 GetTrackMarkBudget(const UObject* WorldContextObject);

	/** Turn placement on or off for this world. Existing marks stay where they are. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static void SetTrackMarksEnabled(const UObject* WorldContextObject, bool bEnabled);

	/** True while this world accepts new marks. */
	UFUNCTION(BlueprintPure, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static bool AreTrackMarksEnabled(const UObject* WorldContextObject);

	/** The measured counters. All zero when there is no subsystem. */
	UFUNCTION(BlueprintPure, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static FTrackMarkStats GetTrackMarkStats(const UObject* WorldContextObject);

	//~ Profiles -----------------------------------------------------------------------------------------

	/** One of the four profiles that exist in code, so you can point a component at Paw without an asset. */
	UFUNCTION(BlueprintPure, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static UTrackMarkProfile* GetBuiltInTrackMarkProfile(const UObject* WorldContextObject, ETrackMarkBuiltInProfile Type);

	/** Profile used when the surface has no mapping and no explicit profile was passed. */
	UFUNCTION(BlueprintPure, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static UTrackMarkProfile* GetDefaultTrackMarkProfile(const UObject* WorldContextObject);

	/** Change that default for this world only. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static void SetDefaultTrackMarkProfile(const UObject* WorldContextObject, UTrackMarkProfile* Profile);

	/** Map a physical material to a profile for this world only. A null profile removes the mapping. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static void SetTrackMarkProfileForSurface(const UObject* WorldContextObject, UPhysicalMaterial* PhysicalMaterial, UTrackMarkProfile* Profile);

	/** Force every placement onto one profile, whatever the surface says. Null goes back to normal resolution. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static void SetTrackMarkProfileOverride(const UObject* WorldContextObject, UTrackMarkProfile* Profile);

	/** The forced profile, or null when surface resolution is in charge. */
	UFUNCTION(BlueprintPure, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static UTrackMarkProfile* GetTrackMarkProfileOverride(const UObject* WorldContextObject);

	//~ Utility ------------------------------------------------------------------------------------------

	/** The world's subsystem, or null. Handy when you want to keep a reference instead of a world context. */
	UFUNCTION(BlueprintPure, Category = "TrackMark", meta = (WorldContext = "WorldContextObject"))
	static UTrackMarkSubsystem* GetTrackMarkSubsystem(const UObject* WorldContextObject);

	/** Scatter marks in a spiral, for demos and for eyeballing a budget. Returns how many actually landed. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Profile"))
	static int32 SpawnTestTrackMarks(
		const UObject* WorldContextObject,
		FVector Origin,
		int32 Count = 200,
		float Radius = 800.0f,
		UTrackMarkProfile* Profile = nullptr);

	/** Human-readable name of a built-in profile, for a button label or a log line. */
	UFUNCTION(BlueprintPure, Category = "TrackMark")
	static FString GetBuiltInTrackMarkProfileName(ETrackMarkBuiltInProfile Type);
};

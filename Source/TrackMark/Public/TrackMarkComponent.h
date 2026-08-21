// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TrackMarkTypes.h"
#include "TrackMarkComponent.generated.h"

class UTrackMarkProfile;

/** Fired after a mark was placed, with the world location it ended up at. Cosmetic hook, local only. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTrackMarkPlaced, FVector, Location, ETrackMarkSide, Side);

/**
 * Hang this on anything that should leave marks on the ground.
 *
 * It is a scene component, so its own transform is the foot position: attach it to a socket and the
 * marks land where the foot lands. Attached to the actor root, marks land under the actor.
 *
 * There are three ways to trigger a mark, and they can be combined:
 *   - an animation notify, through UAnimNotify_TrackMark, which is the accurate one;
 *   - the built-in distance threshold, which needs no animation at all;
 *   - an explicit LeaveTrack() call from Blueprint or C++.
 *
 * In Manual mode the component does not tick. It has no tick function enabled at all, so a thousand of
 * them cost nothing until something calls them.
 */
UCLASS(ClassGroup = (TrackMark), meta = (BlueprintSpawnableComponent, DisplayName = "Track Mark"))
class TRACKMARK_API UTrackMarkComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTrackMarkComponent();

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//~ Configuration ------------------------------------------------------------------------------------

	/** Profile asset for this actor's marks. Empty uses BuiltInProfile instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark")
	TObjectPtr<UTrackMarkProfile> Profile = nullptr;

	/** Code profile used when Profile is empty. This is why a fresh component already leaves boot prints. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark")
	ETrackMarkBuiltInProfile BuiltInProfile = ETrackMarkBuiltInProfile::Boot;

	/**
	 * Let the physical material under the foot pick the profile instead of using this component's.
	 * On, the profile above only applies where the surface has no mapping.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark")
	bool bUseSurfaceProfiles = true;

	/** Manual leaves everything to the caller; Distance drops a mark every stride. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark")
	ETrackMarkTriggerMode TriggerMode = ETrackMarkTriggerMode::Distance;

	/** Alternate left and right feet automatically. Off, every mark is centred on the movement line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark")
	bool bAlternateFeet = true;

	/** Ground distance in centimetres between marks in Distance mode. 0 uses the profile's stride length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|Distance", meta = (ClampMin = "0.0"))
	float StrideLengthOverride = 0.0f;

	/** Sideways offset in centimetres between the two lines of marks. Negative uses the profile's spacing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark", meta = (ClampMin = "-1.0"))
	float LateralSpacingOverride = -1.0f;

	/**
	 * Minimum ground speed in cm/s below which no marks are produced in Distance mode.
	 * Stops a character shuffling in place from stamping the same spot forever.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|Distance", meta = (ClampMin = "0.0"))
	float MinSpeed = 10.0f;

	/** How far above this component the surface trace starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|Trace", meta = (ClampMin = "0.0"))
	float TraceUpDistance = 40.0f;

	/** How far below this component the surface trace reaches. Raise it for a component mounted at the hip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|Trace", meta = (ClampMin = "1.0"))
	float TraceDownDistance = 200.0f;

	/** Multiplies the profile's opacity for every mark this component leaves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float OpacityScale = 1.0f;

	/** Multiplies the profile's mark size for every mark this component leaves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark", meta = (ClampMin = "0.01", ClampMax = "16.0"))
	float SizeScale = 1.0f;

	/** Fired after each successful placement. */
	UPROPERTY(BlueprintAssignable, Category = "TrackMark")
	FOnTrackMarkPlaced OnTrackMarkPlaced;

	//~ Triggers -----------------------------------------------------------------------------------------

	/**
	 * Drop a mark at this component's location, facing the owner's direction of travel.
	 * Call it from an animation notify at the exact frame the foot lands.
	 */
	UFUNCTION(BlueprintCallable, Category = "TrackMark")
	ETrackMarkResult LeaveTrack(ETrackMarkSide Side = ETrackMarkSide::Center);

	/** Drop a mark somewhere else entirely, still using this component's profile and settings. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark")
	ETrackMarkResult LeaveTrackAt(FVector WorldLocation, FVector ForwardDirection, ETrackMarkSide Side = ETrackMarkSide::Center);

	/** Drop the next mark on the other foot than the last one. What Distance mode calls internally. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark")
	ETrackMarkResult LeaveTrackAlternating();

	/** Switch trigger mode at runtime, enabling or disabling this component's tick to match. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark")
	void SetTriggerMode(ETrackMarkTriggerMode NewMode);

	/** Forget where the last mark was, so the next stride is measured from here. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark")
	void ResetStride();

	/** The profile this component would use right now, ignoring any surface mapping. Never null in a live world. */
	UFUNCTION(BlueprintPure, Category = "TrackMark")
	UTrackMarkProfile* GetEffectiveProfile() const;

private:
	/** Fill in a request from this component's configuration. */
	void BuildRequest(FTrackMarkRequest& OutRequest, const FVector& Location, const FVector& Forward, ETrackMarkSide Side) const;

	/** Direction the owner is travelling in, falling back to its facing when it stands still. */
	FVector ResolveForward() const;

	/** Turn this component's tick on or off to match the trigger mode. */
	void UpdateTickEnabled();

	/** Where the last mark was dropped, for the distance threshold. */
	FVector LastMarkLocation = FVector::ZeroVector;

	/** False until the first mark, so the very first stride is measured from where play started. */
	bool bHasLastMarkLocation = false;

	/** Which foot goes next. */
	bool bNextMarkIsLeft = false;
};

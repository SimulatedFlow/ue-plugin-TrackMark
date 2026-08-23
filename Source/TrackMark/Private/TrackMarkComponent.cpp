// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TrackMarkComponent.h"

#include "GameFramework/Actor.h"
#include "TrackMarkLog.h"
#include "TrackMarkProfile.h"
#include "TrackMarkSubsystem.h"

UTrackMarkComponent::UTrackMarkComponent()
{
	// Distance mode is the default because it works on any actor with no animation work at all, but the
	// tick is switched on and off to match the mode, so Manual mode really does cost nothing.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	bWantsInitializeComponent = false;
	bAutoActivate = true;
}

void UTrackMarkComponent::BeginPlay()
{
	Super::BeginPlay();

	LastMarkLocation = GetComponentLocation();
	bHasLastMarkLocation = true;

	UpdateTickEnabled();
}

#if WITH_EDITOR
void UTrackMarkComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTrackMarkComponent, TriggerMode))
	{
		UpdateTickEnabled();
	}
}
#endif

void UTrackMarkComponent::UpdateTickEnabled()
{
	const bool bShouldTick = TriggerMode == ETrackMarkTriggerMode::Distance;
	PrimaryComponentTick.SetTickFunctionEnable(bShouldTick);
}

void UTrackMarkComponent::SetTriggerMode(ETrackMarkTriggerMode NewMode)
{
	if (TriggerMode == NewMode)
	{
		return;
	}

	TriggerMode = NewMode;
	ResetStride();
	UpdateTickEnabled();
}

void UTrackMarkComponent::ResetStride()
{
	LastMarkLocation = GetComponentLocation();
	bHasLastMarkLocation = true;
}

void UTrackMarkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (TriggerMode != ETrackMarkTriggerMode::Distance)
	{
		// Belt and braces: the tick should already be off, but a Blueprint can force it back on.
		return;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (MinSpeed > 0.0f)
	{
		FVector Velocity = Owner->GetVelocity();
		Velocity.Z = 0.0f;
		if (Velocity.SizeSquared() < static_cast<double>(MinSpeed) * static_cast<double>(MinSpeed))
		{
			// Standing still, or shuffling in place. Stamping the same spot forever looks broken.
			return;
		}
	}

	const FVector CurrentLocation = GetComponentLocation();
	if (!bHasLastMarkLocation)
	{
		ResetStride();
		return;
	}

	const UTrackMarkProfile* EffectiveProfile = GetEffectiveProfile();
	const float Stride = StrideLengthOverride > 0.0f
		? StrideLengthOverride
		: (EffectiveProfile ? EffectiveProfile->StrideLength : 75.0f);

	// Ground distance only: walking up a ladder should not stamp footprints into the air.
	FVector Travelled = CurrentLocation - LastMarkLocation;
	Travelled.Z = 0.0f;

	if (Travelled.SizeSquared() < static_cast<double>(Stride) * static_cast<double>(Stride))
	{
		return;
	}

	LeaveTrackAlternating();
}

ETrackMarkResult UTrackMarkComponent::LeaveTrackAlternating()
{
	const ETrackMarkSide Side = bAlternateFeet
		? (bNextMarkIsLeft ? ETrackMarkSide::Left : ETrackMarkSide::Right)
		: ETrackMarkSide::Center;

	bNextMarkIsLeft = !bNextMarkIsLeft;
	return LeaveTrack(Side);
}

ETrackMarkResult UTrackMarkComponent::LeaveTrack(ETrackMarkSide Side)
{
	return LeaveTrackAt(GetComponentLocation(), ResolveForward(), Side);
}

ETrackMarkResult UTrackMarkComponent::LeaveTrackAt(FVector WorldLocation, FVector ForwardDirection, ETrackMarkSide Side)
{
	UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(this);
	if (!Subsystem)
	{
		return ETrackMarkResult::Disabled;
	}

	FTrackMarkRequest Request;
	BuildRequest(Request, WorldLocation, ForwardDirection, Side);

	const ETrackMarkResult Result = Subsystem->PlaceMark(Request);
	if (Result == ETrackMarkResult::Placed)
	{
		LastMarkLocation = WorldLocation;
		bHasLastMarkLocation = true;
		OnTrackMarkPlaced.Broadcast(WorldLocation, Side);
	}
	else if (Result == ETrackMarkResult::NoGround || Result == ETrackMarkResult::TooSteep)
	{
		// The stride still advanced even though nothing was drawn, otherwise a character crossing a gap
		// would fire a request every single frame until it found ground again.
		LastMarkLocation = WorldLocation;
		bHasLastMarkLocation = true;
	}

	return Result;
}

void UTrackMarkComponent::BuildRequest(FTrackMarkRequest& OutRequest, const FVector& Location, const FVector& Forward, ETrackMarkSide Side) const
{
	OutRequest.Location = Location;
	OutRequest.Forward = Forward;
	OutRequest.Profile = Profile;
	OutRequest.bAllowSurfaceProfile = bUseSurfaceProfiles;
	OutRequest.Side = Side;
	OutRequest.bTraceForSurface = true;
	OutRequest.TraceUpDistance = TraceUpDistance;
	OutRequest.TraceDownDistance = TraceDownDistance;
	OutRequest.IgnoreActor = GetOwner();
	OutRequest.OpacityScale = OpacityScale;
	OutRequest.SizeScale = SizeScale;
	OutRequest.LifetimeScale = 1.0f;
	OutRequest.LateralSpacingOverride = LateralSpacingOverride;

	if (!OutRequest.Profile)
	{
		// Resolved here rather than in the subsystem so the component's built-in choice beats the
		// project-wide fallback, which is what a designer picking "Paw" on the component expects.
		if (UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(this))
		{
			OutRequest.Profile = Subsystem->GetBuiltInProfile(BuiltInProfile);
		}
	}
}

FVector UTrackMarkComponent::ResolveForward() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return GetForwardVector();
	}

	FVector Velocity = Owner->GetVelocity();
	Velocity.Z = 0.0f;
	if (!Velocity.IsNearlyZero())
	{
		return Velocity.GetSafeNormal();
	}

	// Standing still: face the way the actor is facing rather than snapping to an arbitrary axis.
	FVector Facing = Owner->GetActorForwardVector();
	Facing.Z = 0.0f;
	return Facing.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
}

UTrackMarkProfile* UTrackMarkComponent::GetEffectiveProfile() const
{
	if (Profile)
	{
		return Profile;
	}

	if (UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(this))
	{
		return Subsystem->GetBuiltInProfile(BuiltInProfile);
	}

	return nullptr;
}

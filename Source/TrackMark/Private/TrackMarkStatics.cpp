// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "TrackMarkStatics.h"

#include "GameFramework/Actor.h"
#include "TrackMarkProfile.h"
#include "TrackMarkSubsystem.h"

ETrackMarkResult UTrackMarkStatics::LeaveTrackMark(
	const UObject* WorldContextObject,
	FVector Location,
	FVector ForwardDirection,
	UTrackMarkProfile* Profile,
	ETrackMarkSide Side,
	AActor* IgnoreActor)
{
	UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject);
	if (!Subsystem)
	{
		return ETrackMarkResult::Disabled;
	}

	FTrackMarkRequest Request;
	Request.Location = Location;
	Request.Forward = ForwardDirection;
	Request.Profile = Profile;
	Request.Side = Side;
	Request.IgnoreActor = IgnoreActor;

	return Subsystem->PlaceMark(Request);
}

ETrackMarkResult UTrackMarkStatics::LeaveTrackMarkForActor(AActor* Actor, UTrackMarkProfile* Profile, ETrackMarkSide Side)
{
	if (!Actor)
	{
		return ETrackMarkResult::Disabled;
	}

	UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(Actor);
	if (!Subsystem)
	{
		return ETrackMarkResult::Disabled;
	}

	FVector Forward = Actor->GetVelocity();
	Forward.Z = 0.0f;
	if (Forward.IsNearlyZero())
	{
		Forward = Actor->GetActorForwardVector();
		Forward.Z = 0.0f;
	}

	FTrackMarkRequest Request;
	Request.Location = Actor->GetActorLocation();
	Request.Forward = Forward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	Request.Profile = Profile;
	Request.Side = Side;
	Request.IgnoreActor = Actor;
	// An actor's origin usually sits at the middle of a capsule, so the trace has to reach further down.
	Request.TraceUpDistance = 60.0f;
	Request.TraceDownDistance = 400.0f;

	return Subsystem->PlaceMark(Request);
}

ETrackMarkResult UTrackMarkStatics::LeaveTrackMarkFromRequest(const UObject* WorldContextObject, const FTrackMarkRequest& Request)
{
	UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->PlaceMark(Request) : ETrackMarkResult::Disabled;
}

void UTrackMarkStatics::ClearAllTrackMarks(const UObject* WorldContextObject)
{
	if (UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject))
	{
		Subsystem->ClearAllMarks();
	}
}

void UTrackMarkStatics::SetTrackMarkBudget(const UObject* WorldContextObject, int32 NewBudget)
{
	if (UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetMarkBudget(NewBudget);
	}
}

int32 UTrackMarkStatics::GetTrackMarkBudget(const UObject* WorldContextObject)
{
	const UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetMarkBudget() : 0;
}

void UTrackMarkStatics::SetTrackMarksEnabled(const UObject* WorldContextObject, bool bEnabled)
{
	if (UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetMarksEnabled(bEnabled);
	}
}

bool UTrackMarkStatics::AreTrackMarksEnabled(const UObject* WorldContextObject)
{
	const UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->AreMarksEnabled() : false;
}

FTrackMarkStats UTrackMarkStatics::GetTrackMarkStats(const UObject* WorldContextObject)
{
	const UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetStats() : FTrackMarkStats();
}

UTrackMarkProfile* UTrackMarkStatics::GetBuiltInTrackMarkProfile(const UObject* WorldContextObject, ETrackMarkBuiltInProfile Type)
{
	UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetBuiltInProfile(Type) : nullptr;
}

UTrackMarkProfile* UTrackMarkStatics::GetDefaultTrackMarkProfile(const UObject* WorldContextObject)
{
	UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetDefaultProfile() : nullptr;
}

void UTrackMarkStatics::SetDefaultTrackMarkProfile(const UObject* WorldContextObject, UTrackMarkProfile* Profile)
{
	if (UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetDefaultProfile(Profile);
	}
}

void UTrackMarkStatics::SetTrackMarkProfileForSurface(const UObject* WorldContextObject, UPhysicalMaterial* PhysicalMaterial, UTrackMarkProfile* Profile)
{
	if (UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetProfileForSurface(PhysicalMaterial, Profile);
	}
}

void UTrackMarkStatics::SetTrackMarkProfileOverride(const UObject* WorldContextObject, UTrackMarkProfile* Profile)
{
	if (UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetProfileOverride(Profile);
	}
}

UTrackMarkProfile* UTrackMarkStatics::GetTrackMarkProfileOverride(const UObject* WorldContextObject)
{
	const UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetProfileOverride() : nullptr;
}

UTrackMarkSubsystem* UTrackMarkStatics::GetTrackMarkSubsystem(const UObject* WorldContextObject)
{
	return UTrackMarkSubsystem::Get(WorldContextObject);
}

int32 UTrackMarkStatics::SpawnTestTrackMarks(
	const UObject* WorldContextObject,
	FVector Origin,
	int32 Count,
	float Radius,
	UTrackMarkProfile* Profile)
{
	UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->SpawnTestMarks(Count, Origin, Radius, Profile) : 0;
}

FString UTrackMarkStatics::GetBuiltInTrackMarkProfileName(ETrackMarkBuiltInProfile Type)
{
	return UTrackMarkProfile::GetBuiltInProfileName(Type);
}

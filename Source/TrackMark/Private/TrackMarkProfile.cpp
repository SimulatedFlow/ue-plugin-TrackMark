// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TrackMarkProfile.h"

#include "Math/UnrealMathUtility.h"

UTrackMarkProfile::UTrackMarkProfile()
{
}

const TCHAR* UTrackMarkProfile::GetBuiltInProfileName(ETrackMarkBuiltInProfile Type)
{
	switch (Type)
	{
	case ETrackMarkBuiltInProfile::Paw:		return TEXT("Paw");
	case ETrackMarkBuiltInProfile::Tyre:	return TEXT("Tyre");
	case ETrackMarkBuiltInProfile::Track:	return TEXT("Track");
	case ETrackMarkBuiltInProfile::Boot:
	default:								return TEXT("Boot");
	}
}

UTrackMarkProfile* UTrackMarkProfile::CreateBuiltIn(ETrackMarkBuiltInProfile Type, UObject* Outer)
{
	// Transient on purpose: these are code profiles, they must never end up saved into a package.
	const FName ProfileName = MakeUniqueObjectName(Outer, UTrackMarkProfile::StaticClass(),
		*FString::Printf(TEXT("TrackMarkProfile_%s"), UTrackMarkProfile::GetBuiltInProfileName(Type)));

	UTrackMarkProfile* Profile = NewObject<UTrackMarkProfile>(Outer, ProfileName, RF_Transient);
	if (!Profile)
	{
		return nullptr;
	}

	// The numbers below are real-world sizes in centimetres, so a mark looks right next to a 180 cm
	// character without anyone having to guess a scale factor.
	switch (Type)
	{
	case ETrackMarkBuiltInProfile::Paw:
		// A medium dog: short, round, close together, and gone quickly because it barely disturbs anything.
		Profile->MarkSize = FVector2D(12.0f, 10.0f);
		Profile->Opacity = 0.55f;
		Profile->Tint = FLinearColor(0.06f, 0.05f, 0.04f, 1.0f);
		Profile->NumVariants = 2;
		Profile->Lifetime = 18.0f;
		Profile->LifetimeJitter = 0.2f;
		Profile->FadeStartFraction = 0.4f;
		Profile->DecayExponent = 1.3f;
		Profile->LateralSpacing = 10.0f;
		Profile->StrideLength = 55.0f;
		Profile->MaxSlopeAngle = 50.0f;
		Profile->YawJitterDegrees = 8.0f;
		break;

	case ETrackMarkBuiltInProfile::Tyre:
		// A car tyre lays down a continuous band, so the stride is short and the marks overlap into a line.
		Profile->MarkSize = FVector2D(40.0f, 22.0f);
		Profile->Opacity = 0.8f;
		Profile->Tint = FLinearColor(0.03f, 0.03f, 0.03f, 1.0f);
		Profile->NumVariants = 1;
		Profile->Lifetime = 45.0f;
		Profile->LifetimeJitter = 0.08f;
		Profile->FadeStartFraction = 0.65f;
		Profile->DecayExponent = 2.0f;
		Profile->LateralSpacing = 150.0f;
		Profile->StrideLength = 22.0f;
		Profile->MaxSlopeAngle = 40.0f;
		Profile->YawJitterDegrees = 0.0f;
		Profile->bMirrorLeftSide = false;
		break;

	case ETrackMarkBuiltInProfile::Track:
		// Tank cleats: wide, heavy, longest lived, and never mirrored because the pattern is symmetric.
		Profile->MarkSize = FVector2D(60.0f, 50.0f);
		Profile->Opacity = 0.85f;
		Profile->Tint = FLinearColor(0.025f, 0.022f, 0.02f, 1.0f);
		Profile->NumVariants = 1;
		Profile->Lifetime = 60.0f;
		Profile->LifetimeJitter = 0.05f;
		Profile->FadeStartFraction = 0.7f;
		Profile->DecayExponent = 2.4f;
		Profile->LateralSpacing = 260.0f;
		Profile->StrideLength = 28.0f;
		Profile->MaxSlopeAngle = 40.0f;
		Profile->YawJitterDegrees = 0.0f;
		Profile->bMirrorLeftSide = false;
		break;

	case ETrackMarkBuiltInProfile::Boot:
	default:
		// Defaults on the class are already the boot print; spelled out here so all four read the same way.
		Profile->MarkSize = FVector2D(26.0f, 11.0f);
		Profile->Opacity = 0.75f;
		Profile->Tint = FLinearColor(0.05f, 0.04f, 0.03f, 1.0f);
		Profile->NumVariants = 2;
		Profile->Lifetime = 25.0f;
		Profile->LifetimeJitter = 0.15f;
		Profile->FadeStartFraction = 0.55f;
		Profile->DecayExponent = 1.6f;
		Profile->LateralSpacing = 14.0f;
		Profile->StrideLength = 75.0f;
		Profile->MaxSlopeAngle = 45.0f;
		Profile->YawJitterDegrees = 4.0f;
		Profile->bMirrorLeftSide = true;
		break;
	}

	return Profile;
}

float UTrackMarkProfile::RollLifetime() const
{
	if (Lifetime <= 0.0f)
	{
		// Zero means the mark never ages out. It still gets recycled once the budget runs round to it.
		return 0.0f;
	}

	if (LifetimeJitter <= 0.0f)
	{
		return Lifetime;
	}

	const float Jitter = FMath::Clamp(LifetimeJitter, 0.0f, 0.9f);
	return Lifetime * FMath::FRandRange(1.0f - Jitter, 1.0f + Jitter);
}

float UTrackMarkProfile::ComputeInverseLifetime(float InLifetime) const
{
	return InLifetime > UE_KINDA_SMALL_NUMBER ? 1.0f / InLifetime : 0.0f;
}

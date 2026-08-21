// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "TrackMarkSettings.h"

#include "TrackMarkLog.h"

namespace TrackMarkSettingsPrivate
{
	/** The engine's own centred 100 x 100 uu plane. Flat, facing +Z, and present in every install. */
	static const TCHAR* DefaultMeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");

	/** Ships with the plugin's content. Missing content falls back to the mesh's material and one warning. */
	static const TCHAR* DefaultMaterialPath = TEXT("/TrackMark/TrackMark/Materials/M_TrackMark.M_TrackMark");
}

UTrackMarkSettings::UTrackMarkSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("TrackMark");

	DefaultMarkMesh = FSoftObjectPath(TrackMarkSettingsPrivate::DefaultMeshPath);
	DefaultMarkMaterial = FSoftObjectPath(TrackMarkSettingsPrivate::DefaultMaterialPath);
}

FName UTrackMarkSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

const UTrackMarkSettings& UTrackMarkSettings::Get()
{
	const UTrackMarkSettings* Settings = GetDefault<UTrackMarkSettings>();
	check(Settings);
	return *Settings;
}

FOnTrackMarkSettingsChanged& UTrackMarkSettings::OnSettingsChanged()
{
	static FOnTrackMarkSettingsChanged Delegate;
	return Delegate;
}

void UTrackMarkSettings::SetTrackMarksEnabled(bool bEnabled)
{
	UTrackMarkSettings* Settings = GetMutableDefault<UTrackMarkSettings>();
	if (!Settings || Settings->bEnableTrackMarks == bEnabled)
	{
		return;
	}

	// Deliberately not saved: a quality slider must not rewrite the shipped configuration.
	Settings->bEnableTrackMarks = bEnabled;
	OnSettingsChanged().Broadcast();
}

void UTrackMarkSettings::SetDefaultMarkBudget(int32 NewBudget)
{
	UTrackMarkSettings* Settings = GetMutableDefault<UTrackMarkSettings>();
	if (!Settings)
	{
		return;
	}

	const int32 Clamped = FMath::Max(1, NewBudget);
	if (Settings->MarkBudget == Clamped)
	{
		return;
	}

	Settings->MarkBudget = Clamped;
	OnSettingsChanged().Broadcast();
}

void UTrackMarkSettings::SetMaxRetirementsPerTick(int32 NewMax)
{
	UTrackMarkSettings* Settings = GetMutableDefault<UTrackMarkSettings>();
	if (!Settings)
	{
		return;
	}

	const int32 Clamped = FMath::Max(1, NewMax);
	if (Settings->MaxRetirementsPerTick == Clamped)
	{
		return;
	}

	Settings->MaxRetirementsPerTick = Clamped;
	OnSettingsChanged().Broadcast();
}

void UTrackMarkSettings::SetSurfaceTraceChannel(TEnumAsByte<ECollisionChannel> NewChannel)
{
	UTrackMarkSettings* Settings = GetMutableDefault<UTrackMarkSettings>();
	if (!Settings || Settings->SurfaceTraceChannel == NewChannel)
	{
		return;
	}

	Settings->SurfaceTraceChannel = NewChannel;
	OnSettingsChanged().Broadcast();
}

void UTrackMarkSettings::SetFallbackBuiltInProfile(ETrackMarkBuiltInProfile NewProfile)
{
	UTrackMarkSettings* Settings = GetMutableDefault<UTrackMarkSettings>();
	if (!Settings || Settings->FallbackBuiltInProfile == NewProfile)
	{
		return;
	}

	Settings->FallbackBuiltInProfile = NewProfile;
	OnSettingsChanged().Broadcast();
}

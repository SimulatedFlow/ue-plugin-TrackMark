// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TrackMark.h"

#include "TrackMarkLog.h"

#define LOCTEXT_NAMESPACE "FTrackMarkModule"

DEFINE_LOG_CATEGORY(LogTrackMark);

void FTrackMarkModule::StartupModule()
{
	UE_LOG(LogTrackMark, Verbose, TEXT("TrackMark runtime module started."));
}

void FTrackMarkModule::ShutdownModule()
{
	UE_LOG(LogTrackMark, Verbose, TEXT("TrackMark runtime module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTrackMarkModule, TrackMark)

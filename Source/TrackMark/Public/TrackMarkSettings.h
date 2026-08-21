// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "TrackMarkTypes.h"
#include "TrackMarkSettings.generated.h"

class UPhysicalMaterial;
class UTrackMarkProfile;

/** Broadcast whenever one of the runtime setters below changes a value, so live subsystems re-read. */
DECLARE_MULTICAST_DELEGATE(FOnTrackMarkSettingsChanged);

/**
 * Project-wide defaults for TrackMark.
 * Edit under Project Settings > Plugins > TrackMark; stored in DefaultGame.ini.
 *
 * Every value that makes sense to change while the game runs also has a Blueprint-callable setter.
 * Those setters write to the in-memory defaults and never touch the .ini, so a quality slider in the
 * options menu cannot corrupt the shipped configuration.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "TrackMark"))
class TRACKMARK_API UTrackMarkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTrackMarkSettings();

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override;

	/** Convenience accessor. Never returns null. */
	static const UTrackMarkSettings& Get();

	/** Fires when a runtime setter changed something. Subsystems listen and re-apply. */
	static FOnTrackMarkSettingsChanged& OnSettingsChanged();

	//~ General ------------------------------------------------------------------------------------------

	/** Master switch. Off, placement calls return Disabled and no batch component is ever created. */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	bool bEnableTrackMarks = true;

	/**
	 * Hard cap on live marks per world. Reaching it does not stop new marks: the oldest one is retired
	 * and its instance slot is handed straight to the new mark, so nothing is allocated after warm-up.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ClampMin = "1", UIMax = "16384"))
	int32 MarkBudget = 2048;

	/** Profile used when the surface has no mapping and no explicit profile was passed. */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	TSoftObjectPtr<UTrackMarkProfile> DefaultProfile;

	/** Code profile used when DefaultProfile is empty. This is why marks appear with zero setup. */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	ETrackMarkBuiltInProfile FallbackBuiltInProfile = ETrackMarkBuiltInProfile::Boot;

	//~ Assets -------------------------------------------------------------------------------------------

	/**
	 * Quad used by every profile that does not name its own mesh. Must be flat in XY, facing +Z, pivot
	 * centred. The engine's own plane fits, which is why it is the default.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Assets", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	FSoftObjectPath DefaultMarkMesh;

	/**
	 * Master material used by every profile that does not name its own. It must read the four custom data
	 * floats; see Docs/DOCUMENTATION.md for the graph. Empty falls back to the mesh's own material, which
	 * renders opaque grey - correct geometry, wrong look, and a warning in the log.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Assets", meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	FSoftObjectPath DefaultMarkMaterial;

	/**
	 * Size in centimetres of the default mesh at scale 1. The engine plane is 100 x 100 uu, hence 100.
	 * The placement code divides the profile's mark size by this to get the instance scale.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Assets", meta = (ClampMin = "0.01"))
	FVector2D ReferenceMeshSize = FVector2D(100.0f, 100.0f);

	//~ Surfaces -----------------------------------------------------------------------------------------

	/**
	 * Physical material under the foot to the profile that should be used there.
	 * A surface that is not listed, or a hit with no physical material at all, falls back to the default
	 * profile. That is not an error and is not logged.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Surfaces")
	TMap<TSoftObjectPtr<UPhysicalMaterial>, TSoftObjectPtr<UTrackMarkProfile>> SurfaceProfiles;

	/** Collision channel used by the downward surface trace. */
	UPROPERTY(Config, EditAnywhere, Category = "Surfaces")
	TEnumAsByte<ECollisionChannel> SurfaceTraceChannel = ECC_WorldStatic;

	/** Trace against complex collision. Off is right for landscapes and almost everything else. */
	UPROPERTY(Config, EditAnywhere, Category = "Surfaces")
	bool bTraceComplex = false;

	//~ Performance --------------------------------------------------------------------------------------

	/**
	 * Upper bound on marks retired by age per tick. Retirement only collapses an instance and hands its
	 * slot back, so this exists to keep a single frame bounded after a long pause, not because it is hot.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "1", UIMax = "4096"))
	int32 MaxRetirementsPerTick = 256;

	/**
	 * Culling headroom on the batch components. Instances are laid flat on the ground far from the batch
	 * origin, so the component bounds need slack or a whole trail pops out at the screen edge.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "1.0", UIMax = "16.0"))
	float BatchBoundsScale = 2.0f;

	/** Distance at which instances start to cull. 0 disables distance culling. */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "0"))
	int32 InstanceStartCullDistance = 0;

	/** Distance at which instances are fully culled. 0 disables distance culling. */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "0"))
	int32 InstanceEndCullDistance = 0;

	/**
	 * Let the subsystem run in a plain editor world as well as in Game and PIE.
	 * The decay clock in the material is the scene's Time input; in an editor viewport that is the editor
	 * world's clock, so marks placed from a construction script age against editor time. Measured in PIE,
	 * documented as untested per-viewport - see Docs/DOCUMENTATION.md.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Performance")
	bool bTickInEditorWorlds = true;

	//~ Runtime setters ----------------------------------------------------------------------------------

	/** Turn the whole system on or off. Off refuses new marks; the ones already down still fade and retire. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark|Settings")
	static void SetTrackMarksEnabled(bool bEnabled);

	/** Change the default budget for worlds created from now on. To change a live world, use UTrackMarkStatics::SetTrackMarkBudget. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark|Settings")
	static void SetDefaultMarkBudget(int32 NewBudget);

	/** Change the retirement ceiling. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark|Settings")
	static void SetMaxRetirementsPerTick(int32 NewMax);

	/** Change the surface trace channel. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark|Settings")
	static void SetSurfaceTraceChannel(TEnumAsByte<ECollisionChannel> NewChannel);

	/** Change the fallback code profile used when no profile asset is configured. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark|Settings")
	static void SetFallbackBuiltInProfile(ETrackMarkBuiltInProfile NewProfile);
};

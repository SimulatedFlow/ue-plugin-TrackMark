// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TrackMarkTypes.h"
#include "TrackMarkProfile.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * Look and behaviour of one kind of ground mark, in one asset.
 *
 * A profile is also the batching key: all marks sharing a profile end up in the same instanced static
 * mesh component, which is exactly one draw call. Two profiles are two draw calls even when they use the
 * same mesh, because each profile gets its own material instance carrying its decay curve and tint.
 *
 * Four profiles exist in code and need no asset at all - see UTrackMarkProfile::CreateBuiltIn and
 * UTrackMarkSubsystem::GetBuiltInProfile. Create an asset when you want to change the numbers, or point
 * a profile at your own mesh and material.
 */
UCLASS(BlueprintType, meta = (DisplayName = "TrackMark Profile"))
class TRACKMARK_API UTrackMarkProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UTrackMarkProfile();

	/**
	 * Build one of the four code profiles. The returned object is transient and outered to Outer;
	 * the subsystem keeps one of each alive for the lifetime of the world.
	 */
	static UTrackMarkProfile* CreateBuiltIn(ETrackMarkBuiltInProfile Type, UObject* Outer);

	/** Human-readable name of a built-in profile, for the console and the stats overlay. */
	static const TCHAR* GetBuiltInProfileName(ETrackMarkBuiltInProfile Type);

	//~ Appearance ---------------------------------------------------------------------------------------

	/**
	 * Mesh the marks are instanced from. Leave empty to use the project's default flat quad.
	 * It must be a flat plane in XY facing +Z with its pivot in the centre, because the placement code
	 * scales X along the direction of travel and Y across it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Master material for these marks. Leave empty to use the project's default M_TrackMark. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Footprint size in centimetres: X is along the direction of travel, Y is across it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "0.1"))
	FVector2D MarkSize = FVector2D(26.0f, 11.0f);

	/** Peak opacity of a fresh mark, before the surface multiplier and before decay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Opacity = 0.75f;

	/** Colour handed to the material's Tint parameter. Dark for mud, light for snow and dust. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor Tint = FLinearColor(0.05f, 0.04f, 0.03f, 1.0f);

	/**
	 * How many shape variants the material can draw, fed to it through the Variant custom data float.
	 * 1 means every mark looks the same. The built-in material picks a variant by rotating and skewing
	 * the shape, so there is no texture atlas to author.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "1", ClampMax = "16"))
	int32 NumVariants = 2;

	/** Mirror the shape for left-foot marks, through the sign of the Variant custom data float. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	bool bMirrorLeftSide = true;

	/** Random yaw wobble in degrees applied per mark, so a straight walk does not look stamped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float YawJitterDegrees = 4.0f;

	//~ Decay --------------------------------------------------------------------------------------------

	/** Seconds a mark lives before it is retired and its slot handed back. 0 means it never ages out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decay", meta = (ClampMin = "0.0", UIMax = "300.0"))
	float Lifetime = 25.0f;

	/** Per-mark random lifetime variation as a fraction of Lifetime, so a whole trail never vanishes at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decay", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float LifetimeJitter = 0.15f;

	/**
	 * Fraction of the lifetime a mark stays at full opacity before it starts to fade.
	 * Written to the material's FadeStart scalar parameter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decay", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FadeStartFraction = 0.55f;

	/**
	 * Shape of the fade after FadeStartFraction, written to the material's DecayExponent parameter.
	 * 1 is linear, above 1 holds the mark longer and then drops away, below 1 fades immediately.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decay", meta = (ClampMin = "0.05", ClampMax = "8.0"))
	float DecayExponent = 1.6f;

	//~ Placement ----------------------------------------------------------------------------------------

	/** Lift above the traced surface point in centimetres. The first line of defence against Z-fighting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0", UIMax = "10.0"))
	float SurfaceOffset = 1.0f;

	/** Extra offset in the mark's own space, applied after the surface offset. X is forward, Y is right. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FVector LocalOffset = FVector::ZeroVector;

	/** Distance in centimetres between the left and the right line of marks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0", UIMax = "300.0"))
	float LateralSpacing = 14.0f;

	/** Ground distance in centimetres between two marks in Distance trigger mode. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "1.0", UIMax = "500.0"))
	float StrideLength = 75.0f;

	/**
	 * Steepest surface in degrees that still takes a mark. Above it the request is rejected and counted,
	 * because a footprint plastered on a wall is worse than no footprint.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxSlopeAngle = 45.0f;

	/** Lay the mark flat along the surface normal. Off keeps it horizontal, which reads badly on ramps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bAlignToSurfaceNormal = true;

	//~ Material parameters ------------------------------------------------------------------------------

	/**
	 * Extra scalar parameters pushed onto this profile's material instance when its batch is created.
	 * Anything the built-in material does not know about is simply ignored by the engine.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Parameters")
	TMap<FName, float> ScalarParameters;

	/** Extra vector parameters pushed onto this profile's material instance when its batch is created. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Parameters")
	TMap<FName, FLinearColor> VectorParameters;

	//~ Helpers ------------------------------------------------------------------------------------------

	/** Lifetime for one mark, with LifetimeJitter applied. Returns 0 for a mark that never ages out. */
	float RollLifetime() const;

	/** 1 / lifetime, or 0 when the mark never ages out. This is what goes into the custom data. */
	float ComputeInverseLifetime(float InLifetime) const;
};

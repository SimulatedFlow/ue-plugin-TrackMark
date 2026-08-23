// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TrackMarkTypes.h"
#include "AnimNotify_TrackMark.generated.h"

class UTrackMarkComponent;

/**
 * Drops a ground mark at the exact animation frame the foot lands.
 *
 * This is the accurate trigger. The distance threshold on UTrackMarkComponent needs no animation work at
 * all, but it cannot know when the foot is actually down; a notify can.
 *
 * Place it on the foot-plant frame of your walk and run montages, set the socket to the foot bone, and
 * pick which side it is. The notify finds the owning actor's UTrackMarkComponent by itself; if the actor
 * has more than one, name the component you mean.
 */
UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "Track Mark"))
class TRACKMARK_API UAnimNotify_TrackMark : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_TrackMark();

	// UAnimNotify interface
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/**
	 * Socket or bone the mark is placed at. Empty uses the track mark component's own location, which is
	 * right when the component is already attached to the foot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TrackMark")
	FName SocketName;

	/** Which foot this notify is for. Drives the lateral offset and the mirrored shape. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TrackMark")
	ETrackMarkSide Side = ETrackMarkSide::Left;

	/**
	 * Which track mark component to use when the actor has several - match against the component name.
	 * Empty takes the first one found.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TrackMark")
	FName ComponentName;

	/** Multiplies the profile's opacity for this notify, so a landing can stamp harder than a walk step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TrackMark", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float OpacityScale = 1.0f;

private:
	/** Find the component this notify should drive on the mesh's owner. */
	UTrackMarkComponent* ResolveComponent(USkeletalMeshComponent* MeshComp) const;
};

// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "AnimNotify_TrackMark.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "TrackMarkComponent.h"

UAnimNotify_TrackMark::UAnimNotify_TrackMark()
{
#if WITH_EDITORONLY_DATA
	// Earth brown, so foot-plant notifies are recognisable at a glance in a montage timeline.
	NotifyColor = FColor(150, 110, 70, 255);
#endif
}

FString UAnimNotify_TrackMark::GetNotifyName_Implementation() const
{
	switch (Side)
	{
	case ETrackMarkSide::Left:	return TEXT("Track Mark (L)");
	case ETrackMarkSide::Right:	return TEXT("Track Mark (R)");
	default:					return TEXT("Track Mark");
	}
}

void UAnimNotify_TrackMark::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UTrackMarkComponent* TrackMarkComponent = ResolveComponent(MeshComp);
	if (!TrackMarkComponent)
	{
		// Silent on purpose: a shared montage played on an actor without the component is a normal case,
		// not a mistake, and one log line per footstep would drown the output log.
		return;
	}

	const float PreviousOpacityScale = TrackMarkComponent->OpacityScale;
	TrackMarkComponent->OpacityScale = PreviousOpacityScale * OpacityScale;

	if (!SocketName.IsNone() && MeshComp)
	{
		const FVector SocketLocation = MeshComp->GetSocketLocation(SocketName);
		FVector Forward = MeshComp->GetOwner() ? MeshComp->GetOwner()->GetVelocity() : FVector::ZeroVector;
		Forward.Z = 0.0f;
		if (Forward.IsNearlyZero() && MeshComp->GetOwner())
		{
			Forward = MeshComp->GetOwner()->GetActorForwardVector();
			Forward.Z = 0.0f;
		}

		// The socket already carries the lateral offset of the real foot, so the profile's spacing would
		// double it up. Centre the mark on the socket instead.
		TrackMarkComponent->LeaveTrackAt(SocketLocation, Forward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector), ETrackMarkSide::Center);
	}
	else
	{
		TrackMarkComponent->LeaveTrack(Side);
	}

	TrackMarkComponent->OpacityScale = PreviousOpacityScale;
}

UTrackMarkComponent* UAnimNotify_TrackMark::ResolveComponent(USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return nullptr;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (ComponentName.IsNone())
	{
		return Owner->FindComponentByClass<UTrackMarkComponent>();
	}

	TArray<UTrackMarkComponent*> Components;
	Owner->GetComponents<UTrackMarkComponent>(Components);
	for (UTrackMarkComponent* Component : Components)
	{
		if (Component && Component->GetFName() == ComponentName)
		{
			return Component;
		}
	}

	return nullptr;
}

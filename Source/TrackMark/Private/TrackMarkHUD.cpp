// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TrackMarkHUD.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "GlobalRenderResources.h"
#include "Misc/StringBuilder.h"
#include "SceneTypes.h"
#include "TrackMarkProfile.h"
#include "TrackMarkSubsystem.h"

const FName ATrackMarkHUD::ButtonId_Profile(TEXT("TrackMark.Profile"));
const FName ATrackMarkHUD::ButtonId_Budget(TEXT("TrackMark.Budget"));
const FName ATrackMarkHUD::ButtonId_Stats(TEXT("TrackMark.Stats"));
const FName ATrackMarkHUD::ButtonId_Clear(TEXT("TrackMark.Clear"));

namespace TrackMarkHudPrivate
{
	static constexpr float LineHeight = 15.0f;
	static constexpr float BoxPadding = 8.0f;
	static constexpr float StatsBoxWidth = 330.0f;
	static constexpr int32 StatsLineCount = 12;

	static const FLinearColor PanelBackground(0.0f, 0.0f, 0.0f, 0.62f);
	static const FLinearColor ButtonBackground(0.09f, 0.11f, 0.14f, 0.88f);
	static const FLinearColor HeadingColor(0.55f, 0.85f, 1.0f, 1.0f);
	static const FLinearColor BodyColor(0.9f, 0.9f, 0.9f, 1.0f);
	static const FLinearColor GoodColor(0.55f, 0.95f, 0.55f, 1.0f);
	static const FLinearColor WarnColor(0.98f, 0.78f, 0.35f, 1.0f);

	static void DrawFilledRect(UCanvas* Canvas, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color)
	{
		FCanvasTileItem Tile(Position, GWhiteTexture, Size, Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}
}

ATrackMarkHUD::ATrackMarkHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATrackMarkHUD::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoEnableMouseInput)
	{
		// Choosing this HUD class is already saying you want to click its buttons. Hit boxes only get
		// clicks when the controller has click events on, so it is switched on here rather than left as
		// a step someone has to remember - the first click has to work.
		if (APlayerController* PlayerController = GetOwningPlayerController())
		{
			PlayerController->bShowMouseCursor = true;
			PlayerController->bEnableClickEvents = true;
			PlayerController->bEnableMouseOverEvents = true;
		}
	}
}

void ATrackMarkHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(this);

	if (bShowStats)
	{
		DrawStatsBox(Canvas, Font, Subsystem);
	}

	if (bShowButtons)
	{
		DrawButtonPanel(Canvas, Font, Subsystem);
	}
}

float ATrackMarkHUD::DrawStatsBox(UCanvas* InCanvas, UFont* Font, const UTrackMarkSubsystem* Subsystem)
{
	using namespace TrackMarkHudPrivate;

	const float BoxHeight = StatsLineCount * LineHeight + BoxPadding * 2.0f;
	DrawFilledRect(
		InCanvas,
		FVector2D(StatsBoxOrigin.X - BoxPadding, StatsBoxOrigin.Y - BoxPadding),
		FVector2D(StatsBoxWidth, BoxHeight),
		PanelBackground);

	float LineY = static_cast<float>(StatsBoxOrigin.Y);
	auto DrawLine = [&](FStringView Line, const FLinearColor& Color)
	{
		FCanvasTextStringViewItem Item(FVector2D(StatsBoxOrigin.X, LineY), Line, Font, Color);
		InCanvas->DrawItem(Item);
		LineY += LineHeight;
	};

	TStringBuilder<192> Line;

	Line.Reset();
	Line.Append(TEXT("TrackMark"));
	DrawLine(Line.ToView(), HeadingColor);

	if (!Subsystem)
	{
		Line.Reset();
		Line.Append(TEXT("no subsystem in this world"));
		DrawLine(Line.ToView(), WarnColor);
		return BoxHeight;
	}

	const FTrackMarkStats& Stats = Subsystem->GetStats();

	Line.Reset();
	Line.Appendf(TEXT("Active marks       %d / %d"), Stats.ActiveMarks, Stats.Budget);
	DrawLine(Line.ToView(), BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Draw calls         %d  (%d batches)"), Stats.ActiveBatches, Stats.Batches);
	DrawLine(Line.ToView(), GoodColor);

	Line.Reset();
	Line.Appendf(TEXT("Instance slots     %d  (%d free)"), Stats.InstanceSlots, Stats.FreeSlots);
	DrawLine(Line.ToView(), BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Slots recycled     %d"), Stats.RecycledSlots);
	DrawLine(Line.ToView(), Stats.RecycledSlots > 0 ? GoodColor : BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Slots allocated    %d"), Stats.AllocatedSlots);
	DrawLine(Line.ToView(), BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Traces this frame  %d"), Stats.TracesLastFrame);
	DrawLine(Line.ToView(), BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Placed / retired   %d / %d"), Stats.PlacedLastFrame, Stats.RetiredLastFrame);
	DrawLine(Line.ToView(), BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Rejected (slope)   %d"), Stats.RejectedBySlopeLastFrame);
	DrawLine(Line.ToView(), Stats.RejectedBySlopeLastFrame > 0 ? WarnColor : BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Subsystem tick     %.3f ms"), Stats.TickMilliseconds);
	DrawLine(Line.ToView(), BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Placement          %.3f ms"), Stats.PlacementMilliseconds);
	DrawLine(Line.ToView(), BodyColor);

	Line.Reset();
	Line.Appendf(TEXT("Marks enabled      %s"), Subsystem->AreMarksEnabled() ? TEXT("yes") : TEXT("no"));
	DrawLine(Line.ToView(), Subsystem->AreMarksEnabled() ? BodyColor : WarnColor);

	return BoxHeight;
}

void ATrackMarkHUD::DrawButtonPanel(UCanvas* InCanvas, UFont* Font, UTrackMarkSubsystem* Subsystem)
{
	using namespace TrackMarkHudPrivate;

	const float PanelLeft = static_cast<float>(InCanvas->SizeX) - ButtonPanelRightMargin - ButtonWidth;
	float ButtonY = ButtonPanelTop;

	if (bShowBuiltInButtons)
	{
		// Every label below is derived from the subsystem on this very frame. Nothing is cached, so the
		// panel cannot claim one thing while the system does another.
		DrawButton(InCanvas, Font, FVector2D(PanelLeft, ButtonY), BuildProfileButtonLabel(Subsystem), ButtonId_Profile, FLinearColor(0.35f, 0.62f, 0.95f, 1.0f));
		ButtonY += ButtonHeight + ButtonSpacing;

		DrawButton(InCanvas, Font, FVector2D(PanelLeft, ButtonY), BuildBudgetButtonLabel(Subsystem), ButtonId_Budget, FLinearColor(0.95f, 0.68f, 0.28f, 1.0f));
		ButtonY += ButtonHeight + ButtonSpacing;

		DrawButton(InCanvas, Font, FVector2D(PanelLeft, ButtonY), FString::Printf(TEXT("STATS: %s"), bShowStats ? TEXT("ON") : TEXT("OFF")), ButtonId_Stats, FLinearColor(0.55f, 0.85f, 0.55f, 1.0f));
		ButtonY += ButtonHeight + ButtonSpacing;

		DrawButton(InCanvas, Font, FVector2D(PanelLeft, ButtonY), TEXT("CLEAR MARKS"), ButtonId_Clear, FLinearColor(0.92f, 0.42f, 0.42f, 1.0f));
		ButtonY += ButtonHeight + ButtonSpacing;
	}

	for (const FTrackMarkHudButton& Button : CustomButtons)
	{
		DrawButton(InCanvas, Font, FVector2D(PanelLeft, ButtonY), Button.Label, Button.Id, Button.Accent);
		ButtonY += ButtonHeight + ButtonSpacing;
	}
}

void ATrackMarkHUD::DrawButton(UCanvas* InCanvas, UFont* Font, const FVector2D& Position, const FString& Label, FName Id, const FLinearColor& Accent)
{
	using namespace TrackMarkHudPrivate;

	DrawFilledRect(InCanvas, Position, FVector2D(ButtonWidth, ButtonHeight), ButtonBackground);
	// A colour stripe down the left edge, so related buttons read as a group at a glance.
	DrawFilledRect(InCanvas, Position, FVector2D(4.0f, ButtonHeight), Accent);

	InCanvas->DrawText(
		Font,
		Label,
		static_cast<float>(Position.X) + 14.0f,
		static_cast<float>(Position.Y) + (ButtonHeight - LineHeight) * 0.5f);

	AddHitBox(Position, FVector2D(ButtonWidth, ButtonHeight), Id, /*bConsumesInput*/ true, /*Priority*/ 1);
}

void ATrackMarkHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);

	UTrackMarkSubsystem* Subsystem = UTrackMarkSubsystem::Get(this);
	HandleBuiltInButton(BoxName, Subsystem);

	// Custom buttons and built-in ones fire the same delegate, so one Blueprint handler can drive the lot.
	OnButtonClicked.Broadcast(BoxName);
}

bool ATrackMarkHUD::HandleBuiltInButton(FName ButtonId, UTrackMarkSubsystem* Subsystem)
{
	if (ButtonId == ButtonId_Stats)
	{
		bShowStats = !bShowStats;
		return true;
	}

	if (!Subsystem)
	{
		return false;
	}

	if (ButtonId == ButtonId_Profile)
	{
		constexpr int32 NumBuiltIns = static_cast<int32>(ETrackMarkBuiltInProfile::Track) + 1;

		// Step from what the subsystem is actually doing, not from a counter of our own.
		const int32 CurrentIndex = FindBuiltInProfileIndex(Subsystem, Subsystem->GetProfileOverride());
		const int32 NextIndex = CurrentIndex + 1;

		if (NextIndex >= NumBuiltIns)
		{
			Subsystem->SetProfileOverride(nullptr);
		}
		else
		{
			Subsystem->SetProfileOverride(Subsystem->GetBuiltInProfile(static_cast<ETrackMarkBuiltInProfile>(NextIndex)));
		}
		return true;
	}

	if (ButtonId == ButtonId_Budget)
	{
		const int32 Low = FMath::Max(1, BudgetToggleLow);
		const int32 High = FMath::Max(Low, BudgetToggleHigh);
		Subsystem->SetMarkBudget(Subsystem->GetMarkBudget() == High ? Low : High);
		return true;
	}

	if (ButtonId == ButtonId_Clear)
	{
		Subsystem->ClearAllMarks();
		return true;
	}

	return false;
}

int32 ATrackMarkHUD::FindBuiltInProfileIndex(UTrackMarkSubsystem* Subsystem, const UTrackMarkProfile* Profile)
{
	if (!Subsystem || !Profile)
	{
		return INDEX_NONE;
	}

	constexpr int32 NumBuiltIns = static_cast<int32>(ETrackMarkBuiltInProfile::Track) + 1;
	for (int32 Index = 0; Index < NumBuiltIns; ++Index)
	{
		if (Subsystem->GetBuiltInProfile(static_cast<ETrackMarkBuiltInProfile>(Index)) == Profile)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

FString ATrackMarkHUD::BuildProfileButtonLabel(const UTrackMarkSubsystem* Subsystem) const
{
	if (!Subsystem)
	{
		return TEXT("PROFILE: -");
	}

	const UTrackMarkProfile* Override = Subsystem->GetProfileOverride();
	if (!Override)
	{
		return TEXT("PROFILE: BY SURFACE");
	}

	// FindBuiltInProfileIndex needs a mutable subsystem because looking a built-in up creates it on
	// first use; here we only need a name, and a profile asset can supply its own.
	const int32 Index = FindBuiltInProfileIndex(const_cast<UTrackMarkSubsystem*>(Subsystem), Override);
	const FString Name = Index != INDEX_NONE
		? FString(UTrackMarkProfile::GetBuiltInProfileName(static_cast<ETrackMarkBuiltInProfile>(Index)))
		: Override->GetName();

	return FString::Printf(TEXT("PROFILE: %s"), *Name.ToUpper());
}

FString ATrackMarkHUD::BuildBudgetButtonLabel(const UTrackMarkSubsystem* Subsystem) const
{
	if (!Subsystem)
	{
		return TEXT("BUDGET: -");
	}

	const int32 Low = FMath::Max(1, BudgetToggleLow);
	const int32 High = FMath::Max(Low, BudgetToggleHigh);
	const int32 Current = Subsystem->GetMarkBudget();
	const int32 Next = Current == High ? Low : High;

	return FString::Printf(TEXT("BUDGET %d  >  %d"), Current, Next);
}

//~ Custom buttons ---------------------------------------------------------------------------------------

void ATrackMarkHUD::AddButton(FName Id, const FString& Label, FLinearColor Accent)
{
	if (Id.IsNone())
	{
		return;
	}

	for (FTrackMarkHudButton& Existing : CustomButtons)
	{
		if (Existing.Id == Id)
		{
			Existing.Label = Label;
			Existing.Accent = Accent;
			return;
		}
	}

	FTrackMarkHudButton NewButton;
	NewButton.Id = Id;
	NewButton.Label = Label;
	NewButton.Accent = Accent;
	CustomButtons.Add(MoveTemp(NewButton));
}

void ATrackMarkHUD::SetButtonLabel(FName Id, const FString& Label)
{
	for (FTrackMarkHudButton& Existing : CustomButtons)
	{
		if (Existing.Id == Id)
		{
			Existing.Label = Label;
			return;
		}
	}
}

void ATrackMarkHUD::RemoveButton(FName Id)
{
	CustomButtons.RemoveAll([Id](const FTrackMarkHudButton& Button) { return Button.Id == Id; });
}

void ATrackMarkHUD::ClearButtons()
{
	CustomButtons.Reset();
}

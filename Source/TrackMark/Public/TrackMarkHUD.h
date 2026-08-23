// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TrackMarkTypes.h"
#include "TrackMarkHUD.generated.h"

class UCanvas;
class UFont;
class UTrackMarkProfile;
class UTrackMarkSubsystem;

/** Fired when any button on the overlay is clicked, built-in ones included. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrackMarkHudButtonClicked, FName, ButtonId);

/** One clickable button on the overlay. Custom entries are added from Blueprint. */
USTRUCT(BlueprintType)
struct TRACKMARK_API FTrackMarkHudButton
{
	GENERATED_BODY()

	/** Identifies the button in OnButtonClicked and in SetButtonLabel. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark")
	FName Id;

	/** What the button says. Change it at runtime with SetButtonLabel. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark")
	FString Label;

	/** Left edge stripe colour, so a group of related buttons reads as a group. */
	UPROPERTY(BlueprintReadWrite, Category = "TrackMark")
	FLinearColor Accent = FLinearColor(0.35f, 0.62f, 0.95f, 1.0f);
};

/**
 * Stats overlay and control panel for TrackMark, drawn on UCanvas from AHUD.
 *
 * Canvas rather than UMG on purpose: this draws in a cooked Shipping build, where DrawDebug is compiled
 * out and a debug widget is usually stripped. It is a real overlay, not a development-only one.
 *
 * The buttons are AHUD hit boxes and answer to real mouse clicks. Their labels are read from the
 * subsystem on every single draw and never cached, so the panel cannot disagree with the system it is
 * controlling - the very first click always does what the label says.
 *
 * Add your own buttons from Blueprint with AddButton and listen on OnButtonClicked; the built-in ones
 * fire the same delegate, so one handler can drive the lot.
 */
UCLASS(Blueprintable, meta = (DisplayName = "TrackMark HUD"))
class TRACKMARK_API ATrackMarkHUD : public AHUD
{
	GENERATED_BODY()

public:
	ATrackMarkHUD();

	// AActor interface
	virtual void BeginPlay() override;

	// AHUD interface
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;

	//~ Configuration ------------------------------------------------------------------------------------

	/** Draw the counters box. Toggled by the STATS button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|HUD")
	bool bShowStats = true;

	/** Draw the button column. Off leaves only the counters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|HUD")
	bool bShowButtons = true;

	/** Draw the four built-in buttons (PROFILE, BUDGET, STATS, CLEAR) above any custom ones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|HUD")
	bool bShowBuiltInButtons = true;

	/**
	 * Show the mouse cursor and enable click events on the owning player controller at BeginPlay.
	 * On by default: choosing this HUD class is already saying you want to click its buttons.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|HUD")
	bool bAutoEnableMouseInput = true;

	/** Low value of the BUDGET toggle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|HUD", meta = (ClampMin = "1"))
	int32 BudgetToggleLow = 256;

	/** High value of the BUDGET toggle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|HUD", meta = (ClampMin = "1"))
	int32 BudgetToggleHigh = 2048;

	/** Top-left corner of the counters box, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|HUD")
	FVector2D StatsBoxOrigin = FVector2D(28.0f, 90.0f);

	/** Distance of the button column from the right edge of the screen, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|HUD", meta = (ClampMin = "0.0"))
	float ButtonPanelRightMargin = 28.0f;

	/** Top edge of the button column, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|HUD", meta = (ClampMin = "0.0"))
	float ButtonPanelTop = 90.0f;

	/** Button width in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|HUD", meta = (ClampMin = "40.0"))
	float ButtonWidth = 232.0f;

	/** Button height in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|HUD", meta = (ClampMin = "16.0"))
	float ButtonHeight = 30.0f;

	/** Vertical gap between two buttons, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackMark|HUD", meta = (ClampMin = "0.0"))
	float ButtonSpacing = 6.0f;

	/** Fired for every button click, built-in or custom. */
	UPROPERTY(BlueprintAssignable, Category = "TrackMark|HUD")
	FOnTrackMarkHudButtonClicked OnButtonClicked;

	//~ Custom buttons -----------------------------------------------------------------------------------

	/** Append a button below the built-in ones. Adding an existing id replaces its label instead. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark|HUD")
	void AddButton(FName Id, const FString& Label, FLinearColor Accent = FLinearColor(0.35f, 0.62f, 0.95f, 1.0f));

	/** Change a custom button's label, which is how a demo shows its own state on the button itself. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark|HUD")
	void SetButtonLabel(FName Id, const FString& Label);

	/** Drop a custom button. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark|HUD")
	void RemoveButton(FName Id);

	/** Drop every custom button. */
	UFUNCTION(BlueprintCallable, Category = "TrackMark|HUD")
	void ClearButtons();

	//~ Built-in button ids ------------------------------------------------------------------------------

	/** Cycles the forced profile through Boot, Paw, Tyre, Track and back to "off". */
	static const FName ButtonId_Profile;
	/** Toggles the live budget between BudgetToggleLow and BudgetToggleHigh. */
	static const FName ButtonId_Budget;
	/** Toggles the counters box. */
	static const FName ButtonId_Stats;
	/** Clears every live mark. */
	static const FName ButtonId_Clear;

protected:
	/** Draw the counters box. Returns the height it used. */
	float DrawStatsBox(UCanvas* InCanvas, UFont* Font, const UTrackMarkSubsystem* Subsystem);

	/** Draw the button column and register one hit box per button. */
	void DrawButtonPanel(UCanvas* InCanvas, UFont* Font, UTrackMarkSubsystem* Subsystem);

	/** Draw one button and register its hit box. */
	void DrawButton(UCanvas* InCanvas, UFont* Font, const FVector2D& Position, const FString& Label, FName Id, const FLinearColor& Accent);

	/** Act on one of the four built-in buttons. Returns false when the id is not one of them. */
	bool HandleBuiltInButton(FName ButtonId, UTrackMarkSubsystem* Subsystem);

	/** Label for the PROFILE button, derived from the subsystem's current override. Never cached. */
	FString BuildProfileButtonLabel(const UTrackMarkSubsystem* Subsystem) const;

	/** Label for the BUDGET button, derived from the subsystem's current budget. Never cached. */
	FString BuildBudgetButtonLabel(const UTrackMarkSubsystem* Subsystem) const;

	/**
	 * Index of Profile among the four built-in ones, or INDEX_NONE when it is not one of them.
	 * The PROFILE button steps through this rather than through a counter of its own, so the label and
	 * the subsystem cannot drift apart - not even when something else changes the override.
	 */
	static int32 FindBuiltInProfileIndex(UTrackMarkSubsystem* Subsystem, const UTrackMarkProfile* Profile);

	/** Custom buttons added from Blueprint, drawn under the built-in ones in insertion order. */
	UPROPERTY(BlueprintReadOnly, Category = "TrackMark|HUD")
	TArray<FTrackMarkHudButton> CustomButtons;
};

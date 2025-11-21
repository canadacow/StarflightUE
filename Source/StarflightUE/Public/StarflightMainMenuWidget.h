// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StarflightMainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UCheckBox;
class UVerticalBox;
class UHorizontalBox;
class UTexture2D;

/**
 * Save game metadata for display in the menu
 */
USTRUCT(BlueprintType)
struct FStarflightSaveGameInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Save Game")
	FString SaveFileName;

	UPROPERTY(BlueprintReadWrite, Category = "Save Game")
	FDateTime Timestamp;

	UPROPERTY(BlueprintReadWrite, Category = "Save Game")
	UTexture2D* Screenshot;

	FStarflightSaveGameInfo()
		: SaveFileName(TEXT(""))
		, Timestamp(FDateTime::MinValue())
		, Screenshot(nullptr)
	{}
};

/**
 * Main menu widget for Starflight game
 * Handles starting/loading/saving games, and displaying options
 */
UCLASS()
class STARFLIGHTUE_API UStarflightMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UStarflightMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	// ============================================
	// UI Bindings (set these up in UMG designer)
	// ============================================
	
	/** Main panel container */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UVerticalBox* MainPanel;

	/** Title text */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* TitleText;

	/** Start new game button */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* StartGameButton;

	/** About/help button */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* AboutButton;

	/** Toggle pause/resume button */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* PauseResumeButton;

	/** Text for pause/resume button */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* PauseResumeText;

	/** Rotoscope graphics checkbox */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCheckBox* RotoscopeCheckBox;

	/** EGA/CGA mode checkbox */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCheckBox* EGAModeCheckBox;

	/** Save game carousel - previous button */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* PreviousSaveButton;

	/** Save game carousel - next button */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* NextSaveButton;

	/** Save game screenshot display */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* SaveGameScreenshot;

	/** Save game timestamp text */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* SaveGameTimestamp;

	/** Load button for selected save */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* LoadSaveButton;

	/** No saves available message */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* NoSavesText;

	/** Performance metrics button */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* PerformanceMetricsButton;

	// ============================================
	// Load Confirmation Dialog Widgets
	// ============================================

	/** Load confirmation panel (hidden by default) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UVerticalBox* LoadConfirmationPanel;

	/** Large screenshot in confirmation dialog */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* ConfirmationScreenshot;

	/** Timestamp in confirmation dialog */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* ConfirmationTimestamp;

	/** OK button in confirmation */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* ConfirmLoadButton;

	/** Cancel button in confirmation */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* CancelLoadButton;

	// ============================================
	// Help/Welcome Panel
	// ============================================

	/** Welcome/help panel (hidden by default) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UVerticalBox* WelcomePanel;

	/** Close welcome button */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* CloseWelcomeButton;

	/** Start game from welcome button */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* WelcomeStartGameButton;

	// ============================================
	// Button Callbacks
	// ============================================

	UFUNCTION()
	void OnStartGameClicked();

	UFUNCTION()
	void OnAboutClicked();

	UFUNCTION()
	void OnPauseResumeClicked();

	UFUNCTION()
	void OnPreviousSaveClicked();

	UFUNCTION()
	void OnNextSaveClicked();

	UFUNCTION()
	void OnLoadSaveClicked();

	UFUNCTION()
	void OnConfirmLoadClicked();

	UFUNCTION()
	void OnCancelLoadClicked();

	UFUNCTION()
	void OnCloseWelcomeClicked();

	UFUNCTION()
	void OnWelcomeStartGameClicked();

	UFUNCTION()
	void OnPerformanceMetricsClicked();

	UFUNCTION()
	void OnRotoscopeCheckChanged(bool bIsChecked);

	UFUNCTION()
	void OnEGAModeCheckChanged(bool bIsChecked);

	// ============================================
	// Save Game Management
	// ============================================

	/** Scan for available save games */
	UFUNCTION(BlueprintCallable, Category = "Starflight")
	void RefreshSaveGameList();

	/** Get the save games directory path */
	UFUNCTION(BlueprintCallable, Category = "Starflight")
	FString GetSaveGamesDirectory() const;

	/** Update UI to show current save game */
	void UpdateSaveGameDisplay();

	/** Show the load confirmation dialog */
	void ShowLoadConfirmation();

	/** Hide the load confirmation dialog */
	void HideLoadConfirmation();

	/** Show the welcome/help panel */
	UFUNCTION(BlueprintCallable, Category = "Starflight")
	void ShowWelcomePanel();

	/** Hide the welcome/help panel */
	UFUNCTION(BlueprintCallable, Category = "Starflight")
	void HideWelcomePanel();

	// ============================================
	// Game Control
	// ============================================

	/** Start a new game */
	UFUNCTION(BlueprintCallable, Category = "Starflight")
	void StartNewGame();

	/** Load a saved game by index */
	UFUNCTION(BlueprintCallable, Category = "Starflight")
	void LoadSavedGame(int32 SaveIndex);

	/** Toggle game pause state */
	UFUNCTION(BlueprintCallable, Category = "Starflight")
	void TogglePause();

	/** Check if game is currently running */
	UFUNCTION(BlueprintCallable, Category = "Starflight")
	bool IsGameRunning() const;

	/** Check if game is paused */
	UFUNCTION(BlueprintCallable, Category = "Starflight")
	bool IsGamePaused() const;

	// ============================================
	// Blueprint Events
	// ============================================

	/** Called when game starts */
	UFUNCTION(BlueprintImplementableEvent, Category = "Starflight")
	void OnGameStarted();

	/** Called when game is loaded */
	UFUNCTION(BlueprintImplementableEvent, Category = "Starflight")
	void OnGameLoaded();

	/** Called when pause state changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "Starflight")
	void OnPauseStateChanged(bool bIsPaused);

private:
	/** List of available save games */
	UPROPERTY()
	TArray<FStarflightSaveGameInfo> SaveGameList;

	/** Currently selected save game index */
	int32 CurrentSaveIndex;

	/** Has save game list been initialized */
	bool bSaveGamesScanned;

	/** Is game currently paused */
	bool bGamePaused;

	/** Helper to create texture from PNG data */
	UTexture2D* CreateTextureFromPNG(const TArray<uint8>& PNGData);

	/** Helper to extract PNG from save file */
	TArray<uint8> ExtractPNGFromSaveFile(const FString& SaveFilePath);
};


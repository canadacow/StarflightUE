#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "StarflightPlayerController.generated.h"

class UStarflightMainMenuWidget;
class AActor;

UCLASS(BlueprintType, Blueprintable)
class STARFLIGHTUE_API AStarflightPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ============================================
    // Game Control
    // ============================================

    /** Start the Starflight game (optionally from a save file) */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    void StartStarflightGame(const FString& SaveFilePath);

    /** Stop the Starflight game */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    void StopStarflightGame();

    /** Pause/unpause the game */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    void SetGamePaused(bool bPaused);

    /** Check if game is currently running */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    bool IsGameRunning() const { return bGameRunning; }

    /** Check if game is currently paused */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    bool IsGamePaused() const { return bGamePaused; }

    // ============================================
    // Graphics Settings
    // ============================================

    /** Set rotoscope (remade) graphics mode */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    void SetRotoscopeMode(bool bEnabled);

    /** Set EGA graphics mode (false = CGA) */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    void SetEGAMode(bool bEnabled);

    /** Get current rotoscope mode state */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    bool GetRotoscopeMode() const { return bRotoscopeEnabled; }

    /** Get current EGA mode state */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    bool GetEGAMode() const { return bEGAEnabled; }

    // ============================================
    // Menu Management
    // ============================================

    /** Toggle the main menu visibility */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    void ToggleMainMenu();

    /** Show the main menu */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    void ShowMainMenu();

    /** Hide the main menu */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    void HideMainMenu();

    /** Get the main menu widget */
    UFUNCTION(BlueprintCallable, Category = "Starflight")
    UStarflightMainMenuWidget* GetMainMenuWidget() const { return MainMenuWidget; }

    // ============================================
    // Widget Class Configuration
    // ============================================

    /** Main menu widget class to spawn */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Starflight")
    TSubclassOf<UStarflightMainMenuWidget> MainMenuWidgetClass;

private:
    bool bShiftDown = false;
    bool bCtrlDown = false;
    bool bAltDown = false;
    bool bSendKeysToEmulator = false;

    // Game state
    bool bGameRunning = false;
    bool bGamePaused = false;
    bool bRotoscopeEnabled = true;
    bool bEGAEnabled = true;

    // Main menu widget instance
    UPROPERTY()
    UStarflightMainMenuWidget* MainMenuWidget;

    void UpdateModifierState(const FKey& Key, bool bPressed);
    void CreateMainMenuWidget();

    // ============================================
    // Camera / View Target Management
    // ============================================

    /** Optional camera actor to use for the Space Station / alternate view */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Starflight|Camera", meta = (AllowPrivateAccess = "true"))
    AActor* StationCamera = nullptr;

    /** Cached default view target so we can return after toggling to StationCamera */
    UPROPERTY()
    AActor* DefaultViewTarget = nullptr;

    /** Whether we are currently viewing through StationCamera */
    bool bUsingStationCamera = false;

    /** Toggle between the default view target and StationCamera */
    void ToggleStationCamera();

    /** Find and cache the StationCamera actor by tag or name if not already set. */
    void ResolveStationCamera();
};

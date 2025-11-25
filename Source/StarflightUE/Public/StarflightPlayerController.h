#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "StarflightPlayerController.generated.h"

class UStarflightMainMenuWidget;
class UUserWidget;
class UImage;
class UMaterialInstanceDynamic;
class UTexture;
class AActor;

UCLASS(BlueprintType, Blueprintable)
class STARFLIGHTUE_API AStarflightPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AStarflightPlayerController();

    virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

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

    /** Cross-fade to a new view target without moving the camera through world space. */
    void CrossfadeToViewTarget(AActor* NewTarget);

    // ============================================
    // Crossfade UI (UMG-backed, C++ driven)
    // ============================================

    /** Widget class providing a full-screen image with M_CameraCrossfade as its brush material. */
    UPROPERTY(EditAnywhere, Category = "Starflight|Camera")
    TSubclassOf<UUserWidget> CameraCrossfadeWidgetClass;

    /** Runtime instance of the crossfade widget. */
    UPROPERTY()
    UUserWidget* CameraCrossfadeWidget = nullptr;

    /** The image inside the widget we apply the dynamic material to (named CrossfadeImage in the UMG asset). */
    UPROPERTY()
    UImage* CameraCrossfadeImage = nullptr;

    /** Dynamic instance of M_CameraCrossfade used for blending the two camera textures. */
    UPROPERTY()
    UMaterialInstanceDynamic* CameraCrossfadeMID = nullptr;

    /** Texture representing the ComputerRoom view (e.g., a render target). */
    UPROPERTY(EditAnywhere, Category = "Starflight|Camera")
    UTexture* ComputerRoomTexture = nullptr;

    /** Texture representing the Station view (e.g., a render target). */
    UPROPERTY(EditAnywhere, Category = "Starflight|Camera")
    UTexture* StationTexture = nullptr;

    /** Current 0..1 blend between the two camera textures. */
    float CrossfadeAlpha = 0.0f;

    /** Total duration of the crossfade, in seconds. */
    float CrossfadeDuration = 0.5f;

    /** True while a crossfade is in progress. */
    bool bCrossfading = false;

    /** Per-frame crossfade update driven from Tick. */
    void TickCrossfade(float DeltaSeconds);

    /** Ensure render targets / scene captures for crossfade exist. */
    void EnsureCrossfadeSetup();

    /** Log the current crossfade setup state for debugging. */
    void LogCrossfadeSetup(const TCHAR* Context) const;
};

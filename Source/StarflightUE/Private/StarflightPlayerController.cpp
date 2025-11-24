#include "StarflightPlayerController.h"
#include "InputCoreTypes.h"
#include "StarflightRuntime/Public/StarflightInput.h"
#include "StarflightRuntime/Public/StarflightBridge.h"
#include "StarflightRuntime/Public/StarflightEmulatorSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "StarflightMainMenuWidget.h"
#include "EngineUtils.h"

bool AStarflightPlayerController::InputKey(const FInputKeyEventArgs& EventArgs)
{
    const FKey Key = EventArgs.Key;
    const bool bPressed = (EventArgs.Event == IE_Pressed || EventArgs.Event == IE_Repeat);

    // Toggle camera/view target with Tab (switch between default view and StationCamera)
    if (bPressed && Key == EKeys::Tab)
    {
        ToggleStationCamera();
        return true;
    }

    // Toggle main menu with Escape
    if (bPressed && Key == EKeys::Escape)
    {
        ToggleMainMenu();
        return true;
    }

    // Toggle emulator capture (use Insert to avoid editor function key bindings)
    if (bPressed && Key == EKeys::Insert) { bSendKeysToEmulator = !bSendKeysToEmulator; return true; }

    if (Key == EKeys::LeftShift || Key == EKeys::RightShift) { UpdateModifierState(Key, bPressed); return bSendKeysToEmulator ? true : Super::InputKey(EventArgs); }
    if (Key == EKeys::LeftControl || Key == EKeys::RightControl) { UpdateModifierState(Key, bPressed); return bSendKeysToEmulator ? true : Super::InputKey(EventArgs); }
    if (Key == EKeys::LeftAlt || Key == EKeys::RightAlt) { UpdateModifierState(Key, bPressed); return bSendKeysToEmulator ? true : Super::InputKey(EventArgs); }

    if (bPressed && bSendKeysToEmulator)
    {
        FStarflightInput::PushKey(Key, bShiftDown, bCtrlDown, bAltDown);
        return true;
    }

    return Super::InputKey(EventArgs);
}

void AStarflightPlayerController::UpdateModifierState(const FKey& Key, bool bPressed)
{
    if (Key == EKeys::LeftShift || Key == EKeys::RightShift) bShiftDown = bPressed;
    else if (Key == EKeys::LeftControl || Key == EKeys::RightControl) bCtrlDown = bPressed;
    else if (Key == EKeys::LeftAlt || Key == EKeys::RightAlt) bAltDown = bPressed;
}

void AStarflightPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Create main menu widget
    CreateMainMenuWidget();
    // Try to auto-bind StationCamera once at startup
    ResolveStationCamera();

    // Show main menu on start
    //ShowMainMenu();

    // Start with UI mode so we can interact with the menu
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    bShowMouseCursor = true;
}

void AStarflightPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopStarflightGame();
    Super::EndPlay(EndPlayReason);
}

// ============================================
// Game Control
// ============================================

void AStarflightPlayerController::StartStarflightGame(const FString& SaveFilePath)
{
    if (!bGameRunning)
    {
        UE_LOG(LogTemp, Log, TEXT("Starting Starflight game..."));
        
        // TODO: Pass save file path to emulator when loading
        StartStarflight();
        
        bGameRunning = true;
        bGamePaused = false;
        bSendKeysToEmulator = true;

        // Switch to game-only input mode
        FInputModeGameOnly Mode;
        Mode.SetConsumeCaptureMouseDown(false);
        SetInputMode(Mode);
        bShowMouseCursor = false;
    }
}

void AStarflightPlayerController::StopStarflightGame()
{
    if (bGameRunning)
    {
        UE_LOG(LogTemp, Log, TEXT("Stopping Starflight game..."));
        StopStarflight();
        bGameRunning = false;
        bGamePaused = false;
        bSendKeysToEmulator = false;
    }
}

void AStarflightPlayerController::SetGamePaused(bool bPaused)
{
    bGamePaused = bPaused;
    
    // TODO: Actually pause the emulator thread
    UE_LOG(LogTemp, Log, TEXT("Game paused: %s"), bPaused ? TEXT("true") : TEXT("false"));
}

// ============================================
// Graphics Settings
// ============================================

void AStarflightPlayerController::SetRotoscopeMode(bool bEnabled)
{
    bRotoscopeEnabled = bEnabled;
    // TODO: Set rotoscope mode in emulator/graphics
    UE_LOG(LogTemp, Log, TEXT("Rotoscope mode: %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void AStarflightPlayerController::SetEGAMode(bool bEnabled)
{
    bEGAEnabled = bEnabled;
    // TODO: Set EGA/CGA mode in emulator/graphics
    UE_LOG(LogTemp, Log, TEXT("EGA mode: %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

// ============================================
// Menu Management
// ============================================

void AStarflightPlayerController::CreateMainMenuWidget()
{
    if (MainMenuWidgetClass && !MainMenuWidget)
    {
        MainMenuWidget = CreateWidget<UStarflightMainMenuWidget>(this, MainMenuWidgetClass);
        if (MainMenuWidget)
        {
            MainMenuWidget->AddToViewport(100); // High Z-order to appear on top
            MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void AStarflightPlayerController::ToggleMainMenu()
{
    if (!MainMenuWidget)
    {
        CreateMainMenuWidget();
    }

    if (MainMenuWidget)
    {
        if (MainMenuWidget->GetVisibility() == ESlateVisibility::Visible)
        {
            HideMainMenu();
        }
        else
        {
            ShowMainMenu();
        }
    }
}

void AStarflightPlayerController::ShowMainMenu()
{
    if (MainMenuWidget)
    {
        MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
        
        // Switch to UI mode so we can interact with menu
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
        SetInputMode(Mode);
        bShowMouseCursor = true;
    }
}

void AStarflightPlayerController::HideMainMenu()
{
    if (MainMenuWidget)
    {
        MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);

        // If game is running, switch back to game-only mode
        if (bGameRunning)
        {
            FInputModeGameOnly Mode;
            Mode.SetConsumeCaptureMouseDown(false);
            SetInputMode(Mode);
            bShowMouseCursor = false;
        }
    }
}

// ============================================
// Camera / View Target Management
// ============================================

void AStarflightPlayerController::ResolveStationCamera()
{
    if (StationCamera)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            const bool bHasTag = It->ActorHasTag(FName(TEXT("StationCamera")));
            const bool bNameMatches = It->GetName().StartsWith(TEXT("StationCamera"));
            if (bHasTag || bNameMatches)
            {
                StationCamera = *It;
                UE_LOG(LogTemp, Log, TEXT("Bound StationCamera to actor: %s"), *StationCamera->GetName());
                break;
            }
        }
    }
}

void AStarflightPlayerController::ToggleStationCamera()
{
    // Query the emulator's current high-level state from the subsystem.
    FStarflightEmulatorState CurrentState = FStarflightEmulatorState::Unknown;
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UStarflightEmulatorSubsystem* EmulatorSubsystem = GameInstance->GetSubsystem<UStarflightEmulatorSubsystem>())
        {
            CurrentState = EmulatorSubsystem->GetCurrentState();
        }
    }

    // If the state is Unknown, always snap back to the ComputerRoom (default view)
    // and do not allow entering the Station camera.
    if (CurrentState == FStarflightEmulatorState::Unknown)
    {
        if (bUsingStationCamera)
        {
            if (!DefaultViewTarget)
            {
                DefaultViewTarget = GetViewTarget();
                if (!DefaultViewTarget)
                {
                    DefaultViewTarget = GetPawn();
                }
            }

            if (DefaultViewTarget)
            {
                SetViewTargetWithBlend(DefaultViewTarget, 0.5f);
            }
            bUsingStationCamera = false;
        }

        // No toggling allowed while the emulator reports an Unknown state.
        return;
    }

    // Only allow transitions when the emulator reports we are in the Station scene.
    if (CurrentState != FStarflightEmulatorState::Station)
    {
        return;
    }

    // Cache the initial view target the first time we toggle
    if (!DefaultViewTarget)
    {
        DefaultViewTarget = GetViewTarget();
        if (!DefaultViewTarget)
        {
            DefaultViewTarget = GetPawn();
        }
    }

    if (!bUsingStationCamera)
    {
        // Lazily auto-bind StationCamera if still not set
        ResolveStationCamera();

        if (!StationCamera)
        {
            UE_LOG(LogTemp, Warning, TEXT("StationCamera is not set on StarflightPlayerController; cannot toggle camera."));
            return;
        }

        SetViewTargetWithBlend(StationCamera, 0.5f);
        bUsingStationCamera = true;
    }
    else
    {
        if (!DefaultViewTarget)
        {
            UE_LOG(LogTemp, Warning, TEXT("DefaultViewTarget is not set; cannot return from station camera."));
            return;
        }

        SetViewTargetWithBlend(DefaultViewTarget, 0.5f);
        bUsingStationCamera = false;
    }
}

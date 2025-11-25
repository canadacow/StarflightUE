#include "StarflightPlayerController.h"
#include "InputCoreTypes.h"
#include "StarflightRuntime/Public/StarflightInput.h"
#include "StarflightRuntime/Public/StarflightBridge.h"
#include "StarflightRuntime/Public/StarflightEmulatorSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "StarflightMainMenuWidget.h"
#include "EngineUtils.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
// Debug: force the crossfade overlay to be visible at all times so we can verify it actually draws.
static constexpr bool GDebugForceCrossfadeAlwaysVisible = false;
// Debug: force the crossfade image to show solid red (bypasses material logic entirely).
static constexpr bool GDebugForceCrossfadeImageRed = false;

AStarflightPlayerController::AStarflightPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;

    // Try to auto-assign the crossfade widget class from content if not set in defaults.
    if (!CameraCrossfadeWidgetClass)
    {
        static ConstructorHelpers::FClassFinder<UUserWidget> CrossfadeClassFinder(
            TEXT("/Game/WBP_CameraCrossfade"));
        if (CrossfadeClassFinder.Succeeded())
        {
            CameraCrossfadeWidgetClass = CrossfadeClassFinder.Class;
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("AStarflightPlayerController: Could not find WBP_CameraCrossfade at /Game/WBP_CameraCrossfade. "
                     "Please set CameraCrossfadeWidgetClass in the controller defaults."));
        }
    }
}

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

    UE_LOG(LogTemp, Warning, TEXT("=== AStarflightPlayerController::BeginPlay START ==="));

    // Create main menu widget
    CreateMainMenuWidget();

    // Create crossfade widget (UMG layout only, logic is in C++)
    UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController: CameraCrossfadeWidgetClass=%s CameraCrossfadeWidget=%p"),
        CameraCrossfadeWidgetClass ? *CameraCrossfadeWidgetClass->GetName() : TEXT("NULL"), CameraCrossfadeWidget);

    if (CameraCrossfadeWidgetClass && !CameraCrossfadeWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController: Creating CameraCrossfadeWidget of class %s"),
            *CameraCrossfadeWidgetClass->GetName());

        CameraCrossfadeWidget = CreateWidget<UUserWidget>(this, CameraCrossfadeWidgetClass);
        UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController: CreateWidget returned %p"), CameraCrossfadeWidget);

        if (CameraCrossfadeWidget)
        {
            // Very high Z order so we are guaranteed to be on top of other viewport widgets.
            const int32 CrossfadeZOrder = 10000;
            UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController: Adding widget to viewport with Z=%d"), CrossfadeZOrder);
            CameraCrossfadeWidget->AddToViewport(CrossfadeZOrder);
            
            const ESlateVisibility DesiredVis = GDebugForceCrossfadeAlwaysVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
            UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController: Setting visibility to %d (GDebugForceCrossfadeAlwaysVisible=%d)"),
                (int32)DesiredVis, GDebugForceCrossfadeAlwaysVisible ? 1 : 0);
            CameraCrossfadeWidget->SetVisibility(DesiredVis);

            UWidget* Found = CameraCrossfadeWidget->GetWidgetFromName(TEXT("CrossfadeImage"));
            if (!Found)
            {
                UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController: Crossfade widget created but no child named 'CrossfadeImage' was found."));
            }
            else
            {
                CameraCrossfadeImage = Cast<UImage>(Found);
                if (!CameraCrossfadeImage)
                {
                    UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController: Widget named 'CrossfadeImage' is not a UImage."));
                }
                else
                {
                    if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(CameraCrossfadeImage->Slot))
                    {
                        CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
                        CanvasSlot->SetOffsets(FMargin(0.0f));
                        CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
                        UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController: Adjusted CrossfadeImage canvas anchors to full-screen."));
                    }

                    UObject* ResObj = CameraCrossfadeImage->Brush.GetResourceObject();
                    UMaterialInterface* BaseMat = Cast<UMaterialInterface>(ResObj);
                    if (!BaseMat)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController: CrossfadeImage brush has no material (resource=%s). Did you assign M_CameraCrossfade?"),
                            ResObj ? *ResObj->GetName() : TEXT("NULL"));
                    }
                    else
                    {
                        CameraCrossfadeMID = UMaterialInstanceDynamic::Create(BaseMat, this);

                        if (GDebugForceCrossfadeImageRed)
                        {
                            FSlateBrush DebugBrush;
                            DebugBrush.DrawAs = ESlateBrushDrawType::Box;
                            DebugBrush.TintColor = FSlateColor(FLinearColor::Red);
                            CameraCrossfadeImage->SetBrush(DebugBrush);
                            CameraCrossfadeWidget->SetRenderOpacity(1.0f);
                            UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController: DEBUG MODE - CrossfadeImage forced to solid RED via SlateBrush."));
                        }
                        else
                        {
                            CameraCrossfadeImage->SetBrushFromMaterial(CameraCrossfadeMID);
                            CameraCrossfadeImage->SetColorAndOpacity(FLinearColor::White);
                            CameraCrossfadeWidget->SetRenderOpacity(1.0f);
                            UE_LOG(LogTemp, Log, TEXT("AStarflightPlayerController: CrossfadeImage and MID initialized using material %s."),
                                *BaseMat->GetName());
                        }
                    }
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController: Failed to create CameraCrossfadeWidget."));
        }
    }

    // First resolve StationCamera, then ensure render targets / scene captures for crossfade
    ResolveStationCamera();
    EnsureCrossfadeSetup();
    LogCrossfadeSetup(TEXT("BeginPlay"));

    // Show main menu on start
    //ShowMainMenu();

    // Start with UI mode so we can interact with the menu
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    bShowMouseCursor = true;

    UE_LOG(LogTemp, Warning, TEXT("=== AStarflightPlayerController::BeginPlay END ==="));
}

void AStarflightPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TickCrossfade(DeltaSeconds);
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

void AStarflightPlayerController::TickCrossfade(float DeltaSeconds)
{
    if (!bCrossfading)
    {
        return;
    }

    if (!CameraCrossfadeMID || !CameraCrossfadeWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController::TickCrossfade: missing MID or widget; aborting crossfade."));
        bCrossfading = false;
        return;
    }

    const float Step = DeltaSeconds / FMath::Max(0.01f, CrossfadeDuration);
    CrossfadeAlpha = FMath::Clamp(CrossfadeAlpha + Step, 0.0f, 1.0f);
    CameraCrossfadeMID->SetScalarParameterValue(TEXT("Blend"), CrossfadeAlpha);

    if (CrossfadeAlpha >= 1.0f - KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Log, TEXT("AStarflightPlayerController: crossfade completed."));
        bCrossfading = false;
        CameraCrossfadeWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void AStarflightPlayerController::LogCrossfadeSetup(const TCHAR* Context) const
{
    const FString Ctx = Context ? FString(Context) : TEXT("Unknown");

    UE_LOG(LogTemp, Log, TEXT("CrossfadeSetup[%s]: WidgetClass=%s Widget=%p Image=%p MID=%p RT_ComputerRoom=%s RT_Station=%s"),
        *Ctx,
        CameraCrossfadeWidgetClass ? *CameraCrossfadeWidgetClass->GetName() : TEXT("NULL"),
        CameraCrossfadeWidget,
        CameraCrossfadeImage,
        CameraCrossfadeMID,
        ComputerRoomTexture ? *ComputerRoomTexture->GetName() : TEXT("NULL"),
        StationTexture ? *StationTexture->GetName() : TEXT("NULL"));

    if (CameraCrossfadeMID)
    {
        UMaterialInterface* BaseMat = CameraCrossfadeMID->GetMaterial();
        const UMaterial* Mat = BaseMat ? BaseMat->GetMaterial() : nullptr;
        if (Mat)
        {
            UE_LOG(LogTemp, Log, TEXT("CrossfadeSetup[%s]: Material=%s Domain=%d BlendMode=%d"),
                *Ctx, *Mat->GetName(), (int32)Mat->MaterialDomain, (int32)Mat->BlendMode);
        }
    }
}

void AStarflightPlayerController::EnsureCrossfadeSetup()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Create render targets if needed
    if (!ComputerRoomTexture)
    {
        UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_ComputerRoom"));
        RT->InitAutoFormat(1280, 720);
        RT->RenderTargetFormat = RTF_RGBA8;
        RT->ClearColor = FLinearColor::Black;
        RT->UpdateResourceImmediate(true);
        ComputerRoomTexture = RT;
        UE_LOG(LogTemp, Log, TEXT("AStarflightPlayerController: Created ComputerRoomTexture render target."));

        // Spawn a capture aligned with the current view
        AActor* ViewActor = GetViewTarget();
        if (!ViewActor)
        {
            ViewActor = GetPawn();
        }
        if (ViewActor)
        {
            ASceneCapture2D* Capture = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(),
                ViewActor->GetActorLocation(), ViewActor->GetActorRotation());
            if (Capture)
            {
                Capture->GetCaptureComponent2D()->TextureTarget = RT;
                Capture->GetCaptureComponent2D()->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
                Capture->GetCaptureComponent2D()->bCaptureEveryFrame = true;
                UE_LOG(LogTemp, Log, TEXT("AStarflightPlayerController: Spawned ComputerRoom SceneCapture2D."));
            }
        }
    }

    if (!StationTexture && StationCamera)
    {
        UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_Station"));
        RT->InitAutoFormat(1280, 720);
        RT->RenderTargetFormat = RTF_RGBA8;
        RT->ClearColor = FLinearColor::Black;
        RT->UpdateResourceImmediate(true);
        StationTexture = RT;
        UE_LOG(LogTemp, Log, TEXT("AStarflightPlayerController: Created StationTexture render target."));

        ASceneCapture2D* Capture = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(),
            StationCamera->GetActorLocation(), StationCamera->GetActorRotation());
        if (Capture)
        {
            Capture->GetCaptureComponent2D()->TextureTarget = RT;
            Capture->GetCaptureComponent2D()->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
            Capture->GetCaptureComponent2D()->bCaptureEveryFrame = true;
            UE_LOG(LogTemp, Log, TEXT("AStarflightPlayerController: Spawned Station SceneCapture2D."));
        }
    }
}

void AStarflightPlayerController::CrossfadeToViewTarget(AActor* NewTarget)
{
    if (!NewTarget)
    {
        return;
    }

    // Make sure our render targets and scene captures exist before attempting to crossfade
    EnsureCrossfadeSetup();
    LogCrossfadeSetup(TEXT("CrossfadeToViewTarget"));

    // Always switch the actual camera immediately (we're blending textures on top)
    SetViewTarget(NewTarget);

    if (!CameraCrossfadeWidget || !CameraCrossfadeImage || !CameraCrossfadeMID)
    {
        UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController::CrossfadeToViewTarget: missing widget/image/MID (Widget=%p, Image=%p, MID=%p); hard cut."),
            CameraCrossfadeWidget, CameraCrossfadeImage, CameraCrossfadeMID);
        return;
    }

    const bool bToStation = (NewTarget == StationCamera);

    // Choose textures based on direction
    UTexture* FromTex = bToStation ? ComputerRoomTexture : StationTexture;
    UTexture* ToTex   = bToStation ? StationTexture   : ComputerRoomTexture;

    if (!FromTex || !ToTex)
    {
        UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController::CrossfadeToViewTarget: FromTex or ToTex is null (From=%s, To=%s); crossfade aborted."),
            FromTex ? *FromTex->GetName() : TEXT("NULL"),
            ToTex ? *ToTex->GetName() : TEXT("NULL"));
        return;
    }

    CrossfadeAlpha = 0.0f;
    bCrossfading   = true;

    CameraCrossfadeMID->SetTextureParameterValue(TEXT("TexA"), FromTex);
    CameraCrossfadeMID->SetTextureParameterValue(TEXT("TexB"), ToTex);
    CameraCrossfadeMID->SetScalarParameterValue(TEXT("Blend"), 0.0f);

    UE_LOG(LogTemp, Log, TEXT("AStarflightPlayerController: starting crossfade %s -> %s (toStation=%s, duration=%.2fs)"),
        *FromTex->GetName(), *ToTex->GetName(), bToStation ? TEXT("true") : TEXT("false"), CrossfadeDuration);

    CameraCrossfadeWidget->SetVisibility(ESlateVisibility::Visible);
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
                CrossfadeToViewTarget(DefaultViewTarget);
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

        CrossfadeToViewTarget(StationCamera);
        bUsingStationCamera = true;
    }
    else
    {
        if (!DefaultViewTarget)
        {
            UE_LOG(LogTemp, Warning, TEXT("DefaultViewTarget is not set; cannot return from station camera."));
            return;
        }

        CrossfadeToViewTarget(DefaultViewTarget);
        bUsingStationCamera = false;
    }
}

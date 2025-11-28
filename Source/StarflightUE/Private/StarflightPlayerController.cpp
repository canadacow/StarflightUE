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
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraTypes.h"
#include "Styling/SlateBrush.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "RenderCore.h"
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

                    const FSlateBrush& ImageBrush = CameraCrossfadeImage->GetBrush();
                    UObject* ResObj = ImageBrush.GetResourceObject();
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
    EnsureCrossfadeSetup();
    UpdateCaptureTransforms();
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
        // Disable scene captures now that the transition is done so they no longer render every frame.
        SetSceneCapturesActive(false);
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

FIntPoint AStarflightPlayerController::GetCrossfadeViewportSize() const
{
    FIntPoint Size(1280, 720);

    if (const UWorld* World = GetWorld())
    {
        if (UGameViewportClient* ViewportClient = World->GetGameViewport())
        {
            if (FViewport* Viewport = ViewportClient->Viewport)
            {
                const FIntPoint ViewportSize = Viewport->GetSizeXY();
                if (ViewportSize.X > 0 && ViewportSize.Y > 0)
                {
                    Size = ViewportSize;
                }
            }
        }
    }

    return Size;
}

void AStarflightPlayerController::ResizeRenderTargetIfNeeded(UTextureRenderTarget2D*& Target, const FIntPoint& DesiredSize, const TCHAR* DebugName)
{
    if (!Target)
    {
        return;
    }

    const int32 Width = FMath::Max(DesiredSize.X, 1);
    const int32 Height = FMath::Max(DesiredSize.Y, 1);

    if (Target->SizeX != Width || Target->SizeY != Height)
    {
        Target->ResizeTarget(Width, Height);
        UE_LOG(LogTemp, Log, TEXT("AStarflightPlayerController: Resized %s render target to %dx%d"),
            DebugName ? DebugName : TEXT("RenderTarget"), Width, Height);
    }
}

void AStarflightPlayerController::UpdateCaptureTransforms()
{
    // Keep both ComputerRoom and Station captures in sync with their respective cameras.

    // ComputerRoom: mirror the final player camera view (PlayerCameraManager) **only while**
    // we are actually in the ComputerRoom view. Once we have switched to the Station camera
    // we intentionally freeze this capture so that its render target continues to represent
    // the "from" view during a Station <-> ComputerRoom crossfade.
    if (ComputerRoomCapture && PlayerCameraManager && !bUsingStationCamera)
    {
        const FVector  ViewLocation = PlayerCameraManager->GetCameraLocation();
        const FRotator ViewRotation = PlayerCameraManager->GetCameraRotation();
        const float    ViewFOV      = PlayerCameraManager->GetFOVAngle();

        ComputerRoomCapture->SetActorLocation(ViewLocation);
        ComputerRoomCapture->SetActorRotation(ViewRotation);

        if (USceneCaptureComponent2D* CaptureComp = ComputerRoomCapture->GetCaptureComponent2D())
        {
            CaptureComp->FOVAngle = ViewFOV;
        }
    }

    // Station: mirror the StationCamera actor.
    if (StationCapture && StationCapture->GetCaptureComponent2D() && StationCamera)
    {
        StationCapture->SetActorLocation(StationCamera->GetActorLocation());
        StationCapture->SetActorRotation(StationCamera->GetActorRotation());

        float StationFOV = 90.0f;
        if (const UCameraComponent* StationCamComp = StationCamera->FindComponentByClass<UCameraComponent>())
        {
            StationFOV = StationCamComp->FieldOfView;
        }
        else if (const ACameraActor* StationCamActor = Cast<ACameraActor>(StationCamera))
        {
            if (const UCameraComponent* CamComp = StationCamActor->GetCameraComponent())
            {
                StationFOV = CamComp->FieldOfView;
            }
        }

        StationCapture->GetCaptureComponent2D()->FOVAngle = StationFOV;
    }
}

void AStarflightPlayerController::SyncCaptureSettingsWithMainView(USceneCaptureComponent2D* CaptureComp) const
{
    if (!CaptureComp)
    {
        return;
    }

    const UWorld* World = GetWorld();
    if (World)
    {
        if (UGameViewportClient* ViewportClient = World->GetGameViewport())
        {
            // Match the main game viewport's show flags so lighting / GI / reflections
            // and other detail settings (shadows, fog, etc.) line up with the player view.
            CaptureComp->ShowFlags = ViewportClient->EngineShowFlags;
        }
    }

    if (PlayerCameraManager)
    {
        // Copy post-process settings (exposure, tone mapping, LUTs, etc.) from the main camera.
        const FMinimalViewInfo& MainPOV = PlayerCameraManager->GetCameraCacheView();
        CaptureComp->PostProcessSettings    = MainPOV.PostProcessSettings;
        CaptureComp->PostProcessBlendWeight = 1.0f;
    }

    // Ensure we use ray tracing when the renderer is configured to do so,
    // so reflections / GI match the main view.
    CaptureComp->bUseRayTracingIfEnabled = true;
}

void AStarflightPlayerController::SetSceneCapturesActive(bool bActive)
{
    auto ConfigureCapture = [bActive](ASceneCapture2D* Capture)
    {
        if (!Capture)
        {
            return;
        }

        if (USceneCaptureComponent2D* CaptureComp = Capture->GetCaptureComponent2D())
        {
            CaptureComp->bCaptureEveryFrame = bActive;
            CaptureComp->bCaptureOnMovement = bActive;
        }
    };

    ConfigureCapture(ComputerRoomCapture);
    ConfigureCapture(StationCapture);
}

void AStarflightPlayerController::EnsureCrossfadeSetup()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FIntPoint DesiredSize = GetCrossfadeViewportSize();
    const int32 Width = FMath::Max(DesiredSize.X, 1);
    const int32 Height = FMath::Max(DesiredSize.Y, 1);

    // Create render targets and scene captures if needed
    if (!ComputerRoomTexture)
    {
        UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_ComputerRoom"));
        RT->InitAutoFormat(Width, Height);
        RT->RenderTargetFormat = RTF_RGBA8;
        RT->ClearColor = FLinearColor::Black;
        RT->UpdateResourceImmediate(true);
        ComputerRoomTexture = RT;
        UE_LOG(LogTemp, Log, TEXT("AStarflightPlayerController: Created ComputerRoomTexture render target."));

        // Spawn a SceneCapture2D that mirrors the active player camera (ComputerRoom view).
        APawn* LocalPawn = GetPawn();
        FVector CamLocation = FVector::ZeroVector;
        FRotator CamRotation = FRotator::ZeroRotator;
        if (PlayerCameraManager)
        {
            CamLocation = PlayerCameraManager->GetCameraLocation();
            CamRotation = PlayerCameraManager->GetCameraRotation();
        }
        else if (LocalPawn)
        {
            CamLocation = LocalPawn->GetActorLocation();
            CamRotation = LocalPawn->GetActorRotation();
        }

        if (UWorld* WW = GetWorld())
        {
            ComputerRoomCapture = WW->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), CamLocation, CamRotation);
            if (ComputerRoomCapture)
            {
                USceneCaptureComponent2D* CaptureComp = ComputerRoomCapture->GetCaptureComponent2D();
                CaptureComp->TextureTarget      = RT;
                CaptureComp->CaptureSource      = ESceneCaptureSource::SCS_FinalColorLDR;
                CaptureComp->bCaptureEveryFrame = false;
                CaptureComp->bCaptureOnMovement = false;
                SyncCaptureSettingsWithMainView(CaptureComp);
                UE_LOG(LogTemp, Log, TEXT("AStarflightPlayerController: Spawned ComputerRoom SceneCapture2D."));
            }
        }
    }
    else
    {
        ResizeRenderTargetIfNeeded(ComputerRoomTexture, DesiredSize, TEXT("ComputerRoomTexture"));
    }

    if (!StationTexture && StationCamera)
    {
        UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_Station"));
        RT->bAutoGenerateMips = false;
        RT->InitAutoFormat(Width, Height);
        RT->RenderTargetFormat = RTF_RGBA8;
        RT->ClearColor = FLinearColor::Black;
        RT->UpdateResourceImmediate(true);
        StationTexture = RT;
        UE_LOG(LogTemp, Log, TEXT("AStarflightPlayerController: Created StationTexture (PF=%s, %dx%d)."),
            *UEnum::GetValueAsString(RT->GetFormat()), Width, Height);

        StationCapture = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(),
            StationCamera->GetActorLocation(), StationCamera->GetActorRotation());
        if (StationCapture)
        {
            USceneCaptureComponent2D* CaptureComp = StationCapture->GetCaptureComponent2D();
            CaptureComp->TextureTarget      = RT;
            CaptureComp->CaptureSource      = ESceneCaptureSource::SCS_FinalColorLDR;
            CaptureComp->bCaptureEveryFrame = false;
            CaptureComp->bCaptureOnMovement = false;
            SyncCaptureSettingsWithMainView(CaptureComp);
            UE_LOG(LogTemp, Log, TEXT("AStarflightPlayerController: Spawned Station SceneCapture2D."));
        }
    }
    else if (StationTexture)
    {
        ResizeRenderTargetIfNeeded(StationTexture, DesiredSize, TEXT("StationTexture"));
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
    UTexture* ToTex   = bToStation ? StationTexture      : ComputerRoomTexture;

    if (!FromTex || !ToTex)
    {
        UE_LOG(LogTemp, Warning, TEXT("AStarflightPlayerController::CrossfadeToViewTarget: FromTex or ToTex is null (From=%s, To=%s); crossfade aborted."),
            FromTex ? *FromTex->GetName() : TEXT("NULL"),
            ToTex ? *ToTex->GetName() : TEXT("NULL"));
        return;
    }

    // Enable scene captures while we are crossfading so they only render during transitions.
    SetSceneCapturesActive(true);

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

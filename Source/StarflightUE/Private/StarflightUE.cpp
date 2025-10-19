#include "StarflightUE.h"
#include "Modules/ModuleManager.h"
#include "StarflightPlayerController.h"

// Primary module
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, StarflightUE, "StarflightUE");

// Pawn with no special input component; controller routes input
AStarflightPawn::AStarflightPawn()
{
}

// GameMode that uses our pawn and custom PlayerController by default
AStarflightGameMode::AStarflightGameMode()
{
    DefaultPawnClass = AStarflightPawn::StaticClass();
    PlayerControllerClass = AStarflightPlayerController::StaticClass();
}

void AStarflightGameMode::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (PC)
    {
        FInputModeGameOnly Mode;
        Mode.SetConsumeCaptureMouseDown(false);
        PC->SetInputMode(Mode);
        PC->bShowMouseCursor = false;
        PC->SetIgnoreLookInput(false);
        PC->SetIgnoreMoveInput(false);
    }
}



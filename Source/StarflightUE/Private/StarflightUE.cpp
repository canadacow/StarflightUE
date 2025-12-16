#include "StarflightUE.h"
#include "Modules/ModuleManager.h"
#include "StarflightPlayerController.h"
#include "EngineUtils.h"
#include "StarflightPlayerStart.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"
#include "Engine/World.h"

// Primary module
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, StarflightUE, "StarflightUE");

namespace
{
    static bool IsComputerRoomMapName(const FString& InMapName)
    {
        FString Clean = InMapName;
#if WITH_EDITOR
        Clean = UWorld::RemovePIEPrefix(Clean);
#endif
        // Handles values like "/Game/ComputerRoom", "ComputerRoom", "UEDPIE_0_ComputerRoom"
        Clean = FPackageName::GetShortName(Clean);
        return Clean.Equals(TEXT("ComputerRoom"), ESearchCase::IgnoreCase);
    }
}

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

void AStarflightGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

#if WITH_EDITOR
    // In PIE, only use Starflight pawn/controller on the ComputerRoom map.
    // Other maps should play like a normal Unreal PIE session.
    if (UWorld* World = GetWorld())
    {
        if (World->WorldType == EWorldType::PIE)
        {
            const bool bIsComputerRoom = IsComputerRoomMapName(MapName);
            if (!bIsComputerRoom)
            {
                PlayerControllerClass = APlayerController::StaticClass();
                DefaultPawnClass = ADefaultPawn::StaticClass();
            }
            else
            {
                PlayerControllerClass = AStarflightPlayerController::StaticClass();
                DefaultPawnClass = AStarflightPawn::StaticClass();
            }
        }
    }
#endif
}

void AStarflightGameMode::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (PC && PC->IsA<AStarflightPlayerController>())
    {
        FInputModeGameOnly Mode;
        Mode.SetConsumeCaptureMouseDown(false);
        PC->SetInputMode(Mode);
        PC->bShowMouseCursor = false;
        PC->SetIgnoreLookInput(false);
        PC->SetIgnoreMoveInput(false);
    }
}

AActor* AStarflightGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AStarflightPlayerStart> It(World); It; ++It)
        {
            return *It;
        }
    }
    return Super::ChoosePlayerStart_Implementation(Player);
}



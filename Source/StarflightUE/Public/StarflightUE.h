#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/GameModeBase.h"
#include "StarflightUE.generated.h"

UCLASS(BlueprintType, Blueprintable)
class STARFLIGHTUE_API AStarflightPawn : public ASpectatorPawn
{
    GENERATED_BODY()
public:
    AStarflightPawn();
};

UCLASS(BlueprintType, Blueprintable)
class STARFLIGHTUE_API AStarflightGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AStarflightGameMode();
    virtual void BeginPlay() override;

    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};

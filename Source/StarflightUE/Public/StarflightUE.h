#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h"
#include "StarflightUE.generated.h"

UCLASS()
class AStarflightPawn : public APawn
{
    GENERATED_BODY()
public:
    AStarflightPawn();
};

UCLASS()
class AStarflightGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AStarflightGameMode();
    virtual void BeginPlay() override;
};

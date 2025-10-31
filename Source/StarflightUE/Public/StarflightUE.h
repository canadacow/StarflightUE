#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h"
#include "StarflightUE.generated.h"

UCLASS(BlueprintType, Blueprintable)
class STARFLIGHTUE_API AStarflightPawn : public APawn
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
};

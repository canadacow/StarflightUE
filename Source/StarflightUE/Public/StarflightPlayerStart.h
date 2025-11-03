#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "StarflightPlayerStart.generated.h"

// Simple marker PlayerStart used to define a deterministic spawn transform
UCLASS(BlueprintType, Blueprintable)
class STARFLIGHTUE_API AStarflightPlayerStart : public APlayerStart
{
    GENERATED_BODY()
};



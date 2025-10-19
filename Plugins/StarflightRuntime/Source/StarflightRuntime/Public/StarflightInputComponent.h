#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StarflightInputComponent.generated.h"

/**
 * Component that captures keyboard input and forwards it to the Starflight emulator.
 * Add this to a Pawn and enable input to receive keyboard events.
 */
UCLASS(ClassGroup=(Starflight), meta=(BlueprintSpawnableComponent))
class STARFLIGHTRUNTIME_API UStarflightInputComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UStarflightInputComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Enable/disable input capture
    UFUNCTION(BlueprintCallable, Category = "Starflight|Input")
    void SetInputEnabled(bool bEnabled);

protected:
    // Input event handlers
    void OnAnyKeyPressed(FKey Key);
    void OnAnyKeyReleased(FKey Key);
    
private:
    // Track shift/ctrl/alt state
    bool bShiftDown = false;
    bool bCtrlDown = false;
    bool bAltDown = false;
    
    // Cached owner for input setup
    APawn* OwnerPawn = nullptr;
    
    // Input component reference
    UInputComponent* BoundInputComponent = nullptr;
};


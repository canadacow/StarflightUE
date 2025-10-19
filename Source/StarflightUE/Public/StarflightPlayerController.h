#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "StarflightPlayerController.generated.h"

UCLASS()
class AStarflightPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
    virtual void BeginPlay() override;

private:
    bool bShiftDown = false;
    bool bCtrlDown = false;
    bool bAltDown = false;

    void UpdateModifierState(const FKey& Key, bool bPressed);
};

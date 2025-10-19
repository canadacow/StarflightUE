#include "StarflightPlayerController.h"
#include "InputCoreTypes.h"
#include "StarflightRuntime/Public/StarflightInput.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

bool AStarflightPlayerController::InputKey(const FInputKeyEventArgs& EventArgs)
{
    const FKey Key = EventArgs.Key;
    const bool bPressed = (EventArgs.Event == IE_Pressed || EventArgs.Event == IE_Repeat);

    if (Key == EKeys::LeftShift || Key == EKeys::RightShift) { UpdateModifierState(Key, bPressed); return true; }
    if (Key == EKeys::LeftControl || Key == EKeys::RightControl) { UpdateModifierState(Key, bPressed); return true; }
    if (Key == EKeys::LeftAlt || Key == EKeys::RightAlt) { UpdateModifierState(Key, bPressed); return true; }

    if (bPressed)
    {
        FStarflightInput::PushKey(Key, bShiftDown, bCtrlDown, bAltDown);
    }

    // Consume all keyboard input so it behaves like a normal game-only app
    return true;
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

    // Ensure this controller owns input and is in Game Only mode
    FInputModeGameOnly Mode;
    Mode.SetConsumeCaptureMouseDown(false);
    SetInputMode(Mode);
    bShowMouseCursor = false;
    SetIgnoreLookInput(false);
    SetIgnoreMoveInput(false);
}

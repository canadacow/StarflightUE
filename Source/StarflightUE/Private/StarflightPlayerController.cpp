#include "StarflightPlayerController.h"
#include "InputCoreTypes.h"
#include "StarflightRuntime/Public/StarflightInput.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

bool AStarflightPlayerController::InputKey(const FInputKeyEventArgs& EventArgs)
{
    __debugbreak();

    const FKey Key = EventArgs.Key;
    const bool bPressed = (EventArgs.Event == IE_Pressed || EventArgs.Event == IE_Repeat);

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
    __debugbreak();

    if (Key == EKeys::LeftShift || Key == EKeys::RightShift) bShiftDown = bPressed;
    else if (Key == EKeys::LeftControl || Key == EKeys::RightControl) bCtrlDown = bPressed;
    else if (Key == EKeys::LeftAlt || Key == EKeys::RightAlt) bAltDown = bPressed;
}

void AStarflightPlayerController::BeginPlay()
{
    Super::BeginPlay();

    __debugbreak();

    // Ensure this controller owns input and is in Game Only mode
    FInputModeGameOnly Mode;
    Mode.SetConsumeCaptureMouseDown(false);
    SetInputMode(Mode);
    bShowMouseCursor = false;
    SetIgnoreLookInput(false);
    SetIgnoreMoveInput(false);
}

#include "StarflightInputPreprocessor.h"
#include "StarflightInput.h"
#include "InputCoreTypes.h"

bool FStarflightInputPreprocessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    const bool bShift = InKeyEvent.IsShiftDown();
    const bool bCtrl = InKeyEvent.IsControlDown();
    const bool bAlt = InKeyEvent.IsAltDown();
    FStarflightInput::PushKey(Key, bShift, bCtrl, bAlt);
    return true; // consume
}

bool FStarflightInputPreprocessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
    return true; // consume to avoid editor shortcuts in PIE
}



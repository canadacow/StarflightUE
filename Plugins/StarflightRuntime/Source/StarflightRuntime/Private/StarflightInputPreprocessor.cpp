#include "StarflightInputPreprocessor.h"

#include "InputCoreTypes.h"
#include "StarflightTravel.h"

bool FStarflightInputPreprocessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// Global Tab behavior:
	// - ComputerRoom + emulator Station state -> travel to FirstPersonStation
	// - FirstPersonStation -> travel back to ComputerRoom
	// Consume only when we actually initiate travel.
	if (Key == EKeys::Tab)
	{
		return StarflightTravel::HandleStationTabTravel();
	}

	return false;
}

bool FStarflightInputPreprocessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	return false;
}



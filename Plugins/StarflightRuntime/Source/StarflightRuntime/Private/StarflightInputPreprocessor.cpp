#include "StarflightInputPreprocessor.h"

#include "StarflightEmulatorSubsystem.h"
#include "InputCoreTypes.h"
#include "StarflightTravel.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

namespace
{
	static FString GetCleanShortMapNameOrEmpty(const UWorld* World)
	{
		if (!World)
		{
			return FString();
		}

		FString Clean = World->GetMapName();
#if WITH_EDITOR
		Clean = UWorld::RemovePIEPrefix(Clean);
#endif
		return FPackageName::GetShortName(Clean);
	}
}

bool FStarflightInputPreprocessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// Global Tab behavior:
	// - ComputerRoom + emulator Station state -> travel to FirstPersonStation
	// - FirstPersonStation -> travel back to ComputerRoom
	// Consume only when we actually initiate travel.
	if (Key == EKeys::Tab)
	{
		// Prefer the active viewport world for determinism after travel.
		UWorld* World = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWorld() : nullptr;

		const bool bHandled = World
			? StarflightTravel::HandleStationTabTravel(World)
			: StarflightTravel::HandleStationTabTravel();

		// No "try": if we're in a known travel map, Tab must work deterministically.
		if (!bHandled)
		{
			const FString MapName = GetCleanShortMapNameOrEmpty(World);

			if (MapName.Equals(TEXT("FirstPersonStation"), ESearchCase::IgnoreCase))
			{
				checkf(false, TEXT("Tab travel failed: expected to travel FirstPersonStation -> ComputerRoom."));
			}

			if (MapName.Equals(TEXT("ComputerRoom"), ESearchCase::IgnoreCase))
			{
				checkf(World, TEXT("Tab travel strict check: World is null in ComputerRoom."));

				UGameInstance* GI = World->GetGameInstance();
				checkf(GI, TEXT("Tab travel strict check: GameInstance is null in ComputerRoom."));

				UStarflightEmulatorSubsystem* Subsystem = GI->GetSubsystem<UStarflightEmulatorSubsystem>();
				checkf(Subsystem, TEXT("Tab travel strict check: UStarflightEmulatorSubsystem missing in ComputerRoom."));

				if (Subsystem->IsInStation())
				{
					checkf(false, TEXT("Tab travel failed: expected to travel ComputerRoom(Station) -> FirstPersonStation."));
				}
			}
		}

		return bHandled;
	}

	return false;
}

bool FStarflightInputPreprocessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	return false;
}



#include "StarflightTravel.h"

#include "StarflightEmulatorSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

namespace
{
	static FString GetCleanShortMapName(const UWorld* World)
	{
		checkf(World, TEXT("GetCleanShortMapName: World is null."));

		FString Clean = World->GetMapName();
#if WITH_EDITOR
		Clean = UWorld::RemovePIEPrefix(Clean);
#endif
		return FPackageName::GetShortName(Clean);
	}

	static void EnsureMapPackageExistsOrDie(const TCHAR* LongPackageName)
	{
		checkf(LongPackageName && LongPackageName[0] != 0, TEXT("EnsureMapPackageExistsOrDie: invalid package name."));

		FString DummyFilename;
		const bool bFound = FPackageName::SearchForPackageOnDisk(LongPackageName, &DummyFilename);
		checkf(bFound, TEXT("Required map package was not found on disk: %s"), LongPackageName);
	}

	static UWorld* ResolveBestGameWorldOrDie()
	{
		checkf(GEngine, TEXT("ResolveBestGameWorldOrDie: GEngine is null."));

		// Prefer the active game viewport world if available. After OpenLevel/level travel,
		// this is the most deterministic way to get the current world for "tab back".
		if (GEngine->GameViewport)
		{
			if (UWorld* ViewportWorld = GEngine->GameViewport->GetWorld())
			{
				return ViewportWorld;
			}
		}

		UWorld* CandidatePIE = nullptr;
		UWorld* CandidateGame = nullptr;

		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			UWorld* World = Ctx.World();
			if (!World)
			{
				continue;
			}

			if (Ctx.WorldType == EWorldType::PIE)
			{
				CandidatePIE = World;
			}
			else if (Ctx.WorldType == EWorldType::Game)
			{
				CandidateGame = World;
			}
		}

		UWorld* Best = CandidatePIE ? CandidatePIE : CandidateGame;
		checkf(Best, TEXT("ResolveBestGameWorldOrDie: no PIE/Game world context was found."));
		return Best;
	}
}

bool StarflightTravel::HandleStationTabTravel()
{
	return HandleStationTabTravel(ResolveBestGameWorldOrDie());
}

bool StarflightTravel::HandleStationTabTravel(UWorld* World)
{
	checkf(World, TEXT("HandleStationTabTravel: World is null."));

	const FString MapName = GetCleanShortMapName(World);

	// ============================================================
	// ComputerRoom -> FirstPersonStation (only when emulator is in Station)
	// ============================================================
	if (MapName.Equals(TEXT("ComputerRoom"), ESearchCase::IgnoreCase))
	{
		UGameInstance* GameInstance = World->GetGameInstance();
		checkf(GameInstance, TEXT("HandleStationTabTravel: GameInstance is null."));

		UStarflightEmulatorSubsystem* EmulatorSubsystem = GameInstance->GetSubsystem<UStarflightEmulatorSubsystem>();
		checkf(EmulatorSubsystem, TEXT("HandleStationTabTravel: UStarflightEmulatorSubsystem is missing from GameInstance."));

		if (!EmulatorSubsystem->IsInStation())
		{
			// Not in Station: Tab is not ours.
			return false;
		}

		EnsureMapPackageExistsOrDie(TEXT("/Game/FirstPersonStation"));
		UGameplayStatics::OpenLevel(World, FName(TEXT("FirstPersonStation")), /*bAbsolute*/ true);
		return true;
	}

	// ============================================================
	// FirstPersonStation -> ComputerRoom (always allowed)
	// ============================================================
	if (MapName.Equals(TEXT("FirstPersonStation"), ESearchCase::IgnoreCase))
	{
		EnsureMapPackageExistsOrDie(TEXT("/Game/ComputerRoom"));
		UGameplayStatics::OpenLevel(World, FName(TEXT("ComputerRoom")), /*bAbsolute*/ true);
		return true;
	}

	return false;
}



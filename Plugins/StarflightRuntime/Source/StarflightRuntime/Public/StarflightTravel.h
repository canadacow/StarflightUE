#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace StarflightTravel
{
	/**
	 * Handle Tab travel between the Station (ComputerRoom) and the 3D Station level (FirstPersonStation).
	 *
	 * Rules:
	 * - If current map is ComputerRoom: only travel if emulator state == Station.
	 * - If current map is FirstPersonStation: always travel back to ComputerRoom.
	 *
	 * This is intentionally strict: it checkf's if required map packages do not exist or if no game world can be resolved.
	 */
	STARFLIGHTRUNTIME_API bool HandleStationTabTravel();
	STARFLIGHTRUNTIME_API bool HandleStationTabTravel(UWorld* World);
}



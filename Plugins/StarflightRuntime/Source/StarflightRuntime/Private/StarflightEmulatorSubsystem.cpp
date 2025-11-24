#include "StarflightEmulatorSubsystem.h"

#include "StarflightBridge.h"
#include "Engine/GameInstance.h"
#include "Misc/ScopeLock.h"
#include "Async/Async.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarflightEmulatorSubsystem, Log, All);

namespace
{
	static const TCHAR* StateToString(FStarflightEmulatorState State)
	{
		switch (State)
		{
		case FStarflightEmulatorState::Off:                      return TEXT("Off");
		case FStarflightEmulatorState::Unknown:                  return TEXT("Unknown");
		case FStarflightEmulatorState::LOGO1:                    return TEXT("LOGO1");
		case FStarflightEmulatorState::LOGO2:                    return TEXT("LOGO2");
		case FStarflightEmulatorState::Station:                  return TEXT("Station");
		case FStarflightEmulatorState::Starmap:                  return TEXT("Starmap");
		case FStarflightEmulatorState::Comms:                    return TEXT("Comms");
		case FStarflightEmulatorState::Encounter:                return TEXT("Encounter");
		case FStarflightEmulatorState::InFlux:                   return TEXT("InFlux");
		case FStarflightEmulatorState::IntrastellarNavigation:   return TEXT("IntrastellarNavigation");
		case FStarflightEmulatorState::InterstellarNavigation:   return TEXT("InterstellarNavigation");
		case FStarflightEmulatorState::Orbiting:                 return TEXT("Orbiting");
		case FStarflightEmulatorState::OrbitLanding:             return TEXT("OrbitLanding");
		case FStarflightEmulatorState::OrbitLanded:              return TEXT("OrbitLanded");
		case FStarflightEmulatorState::OrbitTakeoff:             return TEXT("OrbitTakeoff");
		case FStarflightEmulatorState::GameOps:                  return TEXT("GameOps");
		default:                                                 return TEXT("Unknown");
		}
	}
}

UStarflightEmulatorSubsystem::UStarflightEmulatorSubsystem()
	: bEmulatorRunning(false)
{
}

void UStarflightEmulatorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SetFrameSink([this](const uint8* BGRA, int32 Width, int32 Height, int32 Pitch)
	{
		HandleFrame(BGRA, Width, Height, Pitch);
	});

	SetRotoscopeSink([this](const uint8* BGRA, int32 Width, int32 Height, int32 Pitch)
	{
		HandleRotoscope(BGRA, Width, Height, Pitch);
	});

	SetStatusSink([](const FStarflightStatus& Status)
	{
		// Called from emulator thread; bounce to game thread for UE logging.
		AsyncTask(ENamedThreads::GameThread, [Status]()
		{
			const TCHAR* StateName = StateToString(Status.State);
			UE_LOG(LogStarflightEmulatorSubsystem, Log,
				TEXT("Starflight status: %s (GameContext=%u, LastRunBitTag=%u)"),
				StateName, Status.GameContext, Status.LastRunBitTag);
		});
	});

	StartStarflight();
	bEmulatorRunning.Store(true);

	UE_LOG(LogStarflightEmulatorSubsystem, Log, TEXT("Starflight emulator subsystem initialized and emulator started."));
}

void UStarflightEmulatorSubsystem::Deinitialize()
{
	SetFrameSink(nullptr);
	SetRotoscopeSink(nullptr);
	SetStatusSink(nullptr);

	if (bEmulatorRunning.Load())
	{
		StopStarflight();
		bEmulatorRunning.Store(false);
	}

	{
		FScopeLock FrameLock(&FrameListenersMutex);
		FrameListeners.Reset();
	}
	{
		FScopeLock RotoLock(&RotoscopeListenersMutex);
		RotoscopeListeners.Reset();
	}

	UE_LOG(LogStarflightEmulatorSubsystem, Log, TEXT("Starflight emulator subsystem deinitialized and emulator stopped."));

	Super::Deinitialize();
}

bool UStarflightEmulatorSubsystem::IsEmulatorRunning() const
{
	return bEmulatorRunning.Load();
}

FDelegateHandle UStarflightEmulatorSubsystem::RegisterFrameListener(FStarflightFrameCallback&& Callback)
{
	const FDelegateHandle Handle(FDelegateHandle::GenerateNewHandle);
	FScopeLock Lock(&FrameListenersMutex);
	FrameListeners.Add(FStarflightFrameListenerEntry{ Handle, MoveTemp(Callback) });
	return Handle;
}

void UStarflightEmulatorSubsystem::UnregisterFrameListener(FDelegateHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	FScopeLock Lock(&FrameListenersMutex);
	FrameListeners.RemoveAll([Handle](const FStarflightFrameListenerEntry& Entry)
	{
		return Entry.Handle == Handle;
	});
}

FDelegateHandle UStarflightEmulatorSubsystem::RegisterRotoscopeListener(FStarflightRotoscopeCallback&& Callback)
{
	const FDelegateHandle Handle(FDelegateHandle::GenerateNewHandle);
	FScopeLock Lock(&RotoscopeListenersMutex);
	RotoscopeListeners.Add(FStarflightRotoscopeListenerEntry{ Handle, MoveTemp(Callback) });
	return Handle;
}

void UStarflightEmulatorSubsystem::UnregisterRotoscopeListener(FDelegateHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	FScopeLock Lock(&RotoscopeListenersMutex);
	RotoscopeListeners.RemoveAll([Handle](const FStarflightRotoscopeListenerEntry& Entry)
	{
		return Entry.Handle == Handle;
	});
}

void UStarflightEmulatorSubsystem::HandleFrame(const uint8* BGRA, int32 Width, int32 Height, int32 Pitch)
{
	BroadcastFrame(BGRA, Width, Height, Pitch);
}

void UStarflightEmulatorSubsystem::HandleRotoscope(const uint8* BGRA, int32 Width, int32 Height, int32 Pitch)
{
	BroadcastRotoscope(BGRA, Width, Height, Pitch);
}

void UStarflightEmulatorSubsystem::BroadcastFrame(const uint8* BGRA, int32 Width, int32 Height, int32 Pitch)
{
	TArray<FStarflightFrameListenerEntry> LocalListeners;
	{
		FScopeLock Lock(&FrameListenersMutex);
		LocalListeners = FrameListeners;
	}

	for (const FStarflightFrameListenerEntry& Entry : LocalListeners)
	{
		if (Entry.Callback)
		{
			Entry.Callback(BGRA, Width, Height, Pitch);
		}
	}
}

void UStarflightEmulatorSubsystem::BroadcastRotoscope(const uint8* BGRA, int32 Width, int32 Height, int32 Pitch)
{
	TArray<FStarflightRotoscopeListenerEntry> LocalListeners;
	{
		FScopeLock Lock(&RotoscopeListenersMutex);
		LocalListeners = RotoscopeListeners;
	}

	for (const FStarflightRotoscopeListenerEntry& Entry : LocalListeners)
	{
		if (Entry.Callback)
		{
			Entry.Callback(BGRA, Width, Height, Pitch);
		}
	}
}



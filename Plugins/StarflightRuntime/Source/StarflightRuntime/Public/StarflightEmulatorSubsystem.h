#pragma once

#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "HAL/CriticalSection.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/Atomic.h"
#include "Templates/Function.h"
#include "StarflightBridge.h"
#include "StarflightEmulatorSubsystem.generated.h"

class UStarflightEmulatorSubsystem;

using FStarflightFrameCallback = TFunction<void(const uint8*, int32, int32, int32)>;
using FStarflightRotoscopeCallback = TFunction<void(const uint8*, int32, int32, int32)>;

UCLASS()
class STARFLIGHTRUNTIME_API UStarflightEmulatorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UStarflightEmulatorSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool IsEmulatorRunning() const;

	/** Get the last reported high-level emulator state (game thread only). */
	FStarflightEmulatorState GetCurrentState() const { return LastStatus.State; }

	/** Convenience helper: true when the emulator reports we are in the Station scene. */
	bool IsInStation() const { return LastStatus.State == FStarflightEmulatorState::Station; }

	FDelegateHandle RegisterFrameListener(FStarflightFrameCallback&& Callback);
	void UnregisterFrameListener(FDelegateHandle Handle);

	FDelegateHandle RegisterRotoscopeListener(FStarflightRotoscopeCallback&& Callback);
	void UnregisterRotoscopeListener(FDelegateHandle Handle);

private:
	struct FStarflightFrameListenerEntry
	{
		FDelegateHandle Handle;
		FStarflightFrameCallback Callback;
	};

	struct FStarflightRotoscopeListenerEntry
	{
		FDelegateHandle Handle;
		FStarflightRotoscopeCallback Callback;
	};

	void HandleFrame(const uint8* BGRA, int32 Width, int32 Height, int32 Pitch);
	void HandleRotoscope(const uint8* BGRA, int32 Width, int32 Height, int32 Pitch);

	void BroadcastFrame(const uint8* BGRA, int32 Width, int32 Height, int32 Pitch);
	void BroadcastRotoscope(const uint8* BGRA, int32 Width, int32 Height, int32 Pitch);

	FCriticalSection FrameListenersMutex;
	FCriticalSection RotoscopeListenersMutex;

	TArray<FStarflightFrameListenerEntry> FrameListeners;
	TArray<FStarflightRotoscopeListenerEntry> RotoscopeListeners;

	TAtomic<bool> bEmulatorRunning;

	// Last status reported by the emulator; updated on the game thread only.
	FStarflightStatus LastStatus;
};



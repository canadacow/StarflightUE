#pragma once

#include <functional>
#include <cstdint>

// Start/stop lifetime of the emulator integration
STARFLIGHTRUNTIME_API void StartStarflight();
STARFLIGHTRUNTIME_API void StopStarflight();

// Sinks to receive frames and audio from the emulator
using FrameSinkFn = std::function<void(const uint8_t* bgra, int width, int height, int pitch)>;
using AudioSinkFn = std::function<void(const int16_t* pcm, int frames, int sampleRate, int channels)>;

STARFLIGHTRUNTIME_API void SetFrameSink(FrameSinkFn cb);
STARFLIGHTRUNTIME_API void SetAudioSink(AudioSinkFn cb);

// Internal helper called by graphics.cpp to emit frames
STARFLIGHTRUNTIME_API void EmitFrame(const uint8_t* bgra, int w, int h, int pitch);

// Optional rotoscope debug stream (160x200 BGRA)
using RotoscopeSinkFn = std::function<void(const uint8_t* bgra, int width, int height, int pitch)>;
STARFLIGHTRUNTIME_API void SetRotoscopeSink(RotoscopeSinkFn cb);
STARFLIGHTRUNTIME_API void EmitRotoscope(const uint8_t* bgra, int w, int h, int pitch);

// Space man (astronaut) cursor events
using SpaceManMoveSinkFn = std::function<void(uint16_t pixelX, uint16_t pixelY)>;
STARFLIGHTRUNTIME_API void SetSpaceManMoveSink(SpaceManMoveSinkFn cb);
STARFLIGHTRUNTIME_API void EmitSpaceManMove(uint16_t pixelX, uint16_t pixelY);

// High-level emulator state for gameplay & scripting
enum class FStarflightEmulatorState : uint8_t
{
	Off = 0,
	Unknown,
	LOGO1,
	LOGO2,
	Station,
	Starmap,
	Comms,
	Encounter,
	InFlux,
	IntrastellarNavigation,
	InterstellarNavigation,
	Orbiting,
	OrbitLanding,
	OrbitLanded,
	OrbitTakeoff,
	GameOps,
};

struct FStarflightStatus
{
	FStarflightEmulatorState State = FStarflightEmulatorState::Unknown;
	uint32_t GameContext = 0;     // Copy of frameSync.gameContext
	uint16_t LastRunBitTag = 0;   // Copy of frameSync.lastRunBitTag (RunBitPixel tag)
};

using StatusSinkFn = std::function<void(const FStarflightStatus&)>;

STARFLIGHTRUNTIME_API void SetStatusSink(StatusSinkFn cb);
STARFLIGHTRUNTIME_API void EmitStatus(const FStarflightStatus& status);


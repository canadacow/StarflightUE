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

// Per-texel provenance metadata emitted alongside the rotoscope image
enum class FStarflightRotoContent : uint8_t
{
	Clear = 0,
	Navigational,
	Text,
	Line,
	Ellipse,
	BoxFill,
	PolyFill,
	Pic,
	Plot,
	Tile,
	RunBit,
	AuxSys,
	StarMap,
	SpaceMan,
};

struct FStarflightRotoTexel
{
	uint8_t Content;      // See FStarflightRotoContent
	uint8_t FontNumber;   // 0 if not text
	uint8_t Character;    // Raw CP437 character
	uint8_t Flags;        // Bit 0 = XOR
	int16_t GlyphX;       // Pixel offset within glyph bitmap
	int16_t GlyphY;
	int16_t GlyphWidth;
	int16_t GlyphHeight;
	uint8_t FGColor;      // EGA color index
	uint8_t BGColor;
	uint8_t Reserved0;
	uint8_t Reserved1;
};

using RotoscopeMetaSinkFn = std::function<void(const FStarflightRotoTexel* texels, int width, int height)>;
STARFLIGHTRUNTIME_API void SetRotoscopeMetaSink(RotoscopeMetaSinkFn cb);
STARFLIGHTRUNTIME_API void EmitRotoscopeMeta(const FStarflightRotoTexel* texels, int w, int h);

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


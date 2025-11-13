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



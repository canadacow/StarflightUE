#pragma once

#include <functional>
#include <cstdint>

// Start/stop lifetime of the emulator integration
void StartStarflight();
void StopStarflight();

// Sinks to receive frames and audio from the emulator
using FrameSinkFn = std::function<void(const uint8_t* bgra, int width, int height, int pitch)>;
using AudioSinkFn = std::function<void(const int16_t* pcm, int frames, int sampleRate, int channels)>;

void SetFrameSink(FrameSinkFn cb);
void SetAudioSink(AudioSinkFn cb);



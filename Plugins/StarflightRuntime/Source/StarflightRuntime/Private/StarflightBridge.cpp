#include "StarflightBridge.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
	std::mutex gSinksMutex;
	FrameSinkFn gFrameSink;
	AudioSinkFn gAudioSink;
	std::atomic<bool> gRunning{ false };
	std::thread gWorker;
}

// Forward declarations for internal emit helpers
static inline void EmitFrame(const uint8_t* bgra, int w, int h, int pitch);
static inline void EmitAudio(const int16_t* pcm, int frames, int rate, int channels);

void SetFrameSink(FrameSinkFn cb)
{
	std::lock_guard<std::mutex> lock(gSinksMutex);
	gFrameSink = std::move(cb);
}

void SetAudioSink(AudioSinkFn cb)
{
	std::lock_guard<std::mutex> lock(gSinksMutex);
	gAudioSink = std::move(cb);
}

void StartStarflight()
{
	gRunning.store(true, std::memory_order_release);

	// Dummy 60Hz checkerboard generator to prove the pipeline
	gWorker = std::thread([](){
        const int W = 640;
        const int H = 360;
        const int Pitch = W * 4;
		std::vector<uint8_t> buffer(static_cast<size_t>(H) * Pitch);
		int frame = 0;
		while (gRunning.load(std::memory_order_acquire))
		{
			for (int y = 0; y < H; ++y)
			{
				uint8_t* row = buffer.data() + y * Pitch;
				for (int x = 0; x < W; ++x)
				{
                    const bool a = (((x >> 5) ^ (y >> 5) ^ (frame >> 3)) & 1) == 0;
                    const uint8_t r = a ? 0xFF : 0x00;
                    const uint8_t g = a ? 0x00 : 0xFF;
                    const uint8_t b = 0x00;
                    row[x * 4 + 0] = b; // B
                    row[x * 4 + 1] = g; // G
                    row[x * 4 + 2] = r; // R
					row[x * 4 + 3] = 0xFF;           // A
				}
			}
			EmitFrame(buffer.data(), W, H, Pitch);
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
			++frame;
		}
	});
}

void StopStarflight()
{
	gRunning.store(false, std::memory_order_release);
	if (gWorker.joinable()) gWorker.join();
}

// Internal helpers to emit data (call these from emulator thread once wired)
static inline void EmitFrame(const uint8_t* bgra, int w, int h, int pitch)
{
	FrameSinkFn sink;
	{
		std::lock_guard<std::mutex> lock(gSinksMutex);
		sink = gFrameSink;
	}
	if (sink) { sink(bgra, w, h, pitch); }
}

static inline void EmitAudio(const int16_t* pcm, int frames, int rate, int channels)
{
	AudioSinkFn sink;
	{
		std::lock_guard<std::mutex> lock(gSinksMutex);
		sink = gAudioSink;
	}
	if (sink) { sink(pcm, frames, rate, channels); }
}



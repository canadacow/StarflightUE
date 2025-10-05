// Disable warnings for emulator code
#pragma warning(disable: 4883) // function size suppresses optimizations

#include "StarflightBridge.h"
#include "cpu/cpu.h"
#include "call.h"
#include "graphics.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

namespace
{
	std::mutex gSinksMutex;
	FrameSinkFn gFrameSink;
	AudioSinkFn gAudioSink;
	std::atomic<bool> gRunning{ false };
	std::thread gWorker;
	std::thread gGraphicsThread;
}

// Forward declaration for internal emit helper
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

	// Initialize CPU and memory FIRST (must be done before any graphics access)
	InitCPU();
	
	// Initialize graphics
	GraphicsInit();

	// Start emulator thread
	gWorker = std::thread([](){
		// TODO: InitEmulator needs game data files - commented out for now
		// InitEmulator("");  // Empty path for now, will need game data later

		// For now, just run the graphics update loop without game logic
		enum RETURNCODE ret = OK;
		while (gRunning.load(std::memory_order_acquire) && !IsGraphicsShutdown())
		{
			// TODO: Uncomment when game data is available
			// ret = Step();
			// if (ret != OK && ret != EXIT) {
			// 	break;
			// }
			std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
		}
	});

	// Start graphics update thread (60Hz)
	gGraphicsThread = std::thread([](){
		while (gRunning.load(std::memory_order_acquire) && !IsGraphicsShutdown())
		{
			GraphicsUpdate();
			std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
		}
	});
}

void StopStarflight()
{
	GraphicsQuit();
	gRunning.store(false, std::memory_order_release);
	if (gWorker.joinable()) gWorker.join();
	if (gGraphicsThread.joinable()) gGraphicsThread.join();
}

// Internal helpers to emit data (call these from emulator thread once wired)
void EmitFrame(const uint8_t* bgra, int w, int h, int pitch)
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



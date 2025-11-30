// Disable warnings for emulator code
#pragma warning(disable: 4883) // function size suppresses optimizations

#include "StarflightBridge.h"
#include "cpu/cpu.h"
#include "call.h"
#include "graphics.h"
#include "Misc/Paths.h"
#include "Logging/LogMacros.h"
#include "HAL/PlatformTLS.h"
#include "../Emulator/platform.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <string>

// Global variable to hold project directory for emulator file loading
std::string g_ProjectDirectory;

namespace
{
	std::mutex gSinksMutex;
	FrameSinkFn gFrameSink;
	AudioSinkFn gAudioSink;
	static RotoscopeSinkFn gRotoSink;
	static RotoscopeMetaSinkFn gRotoMetaSink;
	SpaceManMoveSinkFn gSpaceManSink;
	StatusSinkFn gStatusSink;
	FStarflightStatus gLastStatus{ FStarflightEmulatorState::Off, 0u, 0u };
	std::atomic<bool> gRunning{ false };
	std::thread gWorker;
	std::thread gGraphicsThread;
}

DEFINE_LOG_CATEGORY_STATIC(LogStarflightBridge, Log, All);
#define SF_LOG(Format, ...) UE_LOG(LogStarflightBridge, Log, Format, ##__VA_ARGS__)

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

void SetRotoscopeSink(RotoscopeSinkFn cb)
{
	std::lock_guard<std::mutex> lock(gSinksMutex);
	gRotoSink = std::move(cb);
}

void SetRotoscopeMetaSink(RotoscopeMetaSinkFn cb)
{
	std::lock_guard<std::mutex> lock(gSinksMutex);
	gRotoMetaSink = std::move(cb);
}

void StartStarflight()
{
	bool bExpected = false;
	if (!gRunning.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
	{
		SF_LOG(TEXT("StartStarflight called while emulator already running."));
		return;
	}

	// Set project directory for emulator file loading (convert to absolute path)
	FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	g_ProjectDirectory = std::string(TCHAR_TO_UTF8(*ProjectDir));

	// Initialize CPU and memory FIRST (must be done before any graphics access)
	InitCPU();
	
	// Initialize graphics
	GraphicsInit();

	// Start emulator thread
	gWorker = std::thread([](){
		SetCurrentThreadName("Starflight Emulator");
		SF_LOG(TEXT("Emulator thread started (id=%u)"), FPlatformTLS::GetCurrentThreadId());
		
		InitEmulator("");  // Load game data from starflt1-in directory

		enum RETURNCODE ret = OK;
		do
		{
			ret = Step();

			if (IsGraphicsShutdown())
				break;

			if (!gRunning.load(std::memory_order_acquire)) {
				break;
			}
		} while (ret == OK || ret == EXIT);
		
		SF_LOG(TEXT("Emulator thread terminating (id=%u)"), FPlatformTLS::GetCurrentThreadId());
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
	if (!gRunning.exchange(false, std::memory_order_acq_rel))
	{
		SF_LOG(TEXT("StopStarflight called but emulator was not running."));
		return;
	}

	GraphicsQuit();
	if (gWorker.joinable()) gWorker.join();
	if (gGraphicsThread.joinable()) gGraphicsThread.join();

	// Report that the emulator is now off
	FStarflightStatus status;
	status.State = FStarflightEmulatorState::Off;
	status.GameContext = 0;
	status.LastRunBitTag = 0;
	EmitStatus(status);
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

void SetStatusSink(StatusSinkFn cb)
{
	StatusSinkFn sink;
	FStarflightStatus statusSnapshot;
	{
		std::lock_guard<std::mutex> lock(gSinksMutex);
		gStatusSink = std::move(cb);
		sink = gStatusSink;
		statusSnapshot = gLastStatus;
	}

	// Immediately inform new listeners of the current status (default Off)
	if (sink)
	{
		sink(statusSnapshot);
	}
}

void EmitStatus(const FStarflightStatus& status)
{
	StatusSinkFn sink;
	{
		std::lock_guard<std::mutex> lock(gSinksMutex);
		sink = gStatusSink;
		gLastStatus = status;
	}
	if (sink)
	{
		sink(status);
	}
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

void EmitRotoscope(const uint8_t* bgra, int w, int h, int pitch)
{
	RotoscopeSinkFn sink;
	{
		std::lock_guard<std::mutex> lock(gSinksMutex);
		sink = gRotoSink;
	}
	if (sink) { sink(bgra, w, h, pitch); }
}

void EmitRotoscopeMeta(const FStarflightRotoTexel* texels, int w, int h)
{
	RotoscopeMetaSinkFn sink;
	{
		std::lock_guard<std::mutex> lock(gSinksMutex);
		sink = gRotoMetaSink;
	}
	if (sink) { sink(texels, w, h); }
}

void SetSpaceManMoveSink(SpaceManMoveSinkFn cb)
{
	SpaceManMoveSinkFn NewSink;
	{
		std::lock_guard<std::mutex> lock(gSinksMutex);
		gSpaceManSink = std::move(cb);
		NewSink = gSpaceManSink;
	}

	SF_LOG(TEXT("SetSpaceManMoveSink: sink %s"),
		NewSink ? TEXT("BOUND") : TEXT("CLEARED"));
}

void EmitSpaceManMove(uint16_t pixelX, uint16_t pixelY)
{
	SpaceManMoveSinkFn sink;
	{
		std::lock_guard<std::mutex> lock(gSinksMutex);
		sink = gSpaceManSink;
	}
	if (sink)
	{
		SF_LOG(TEXT("EmitSpaceManMove: dispatching (%u, %u) to sink"), pixelX, pixelY);
		sink(pixelX, pixelY);
	}
	else
	{
		SF_LOG(TEXT("EmitSpaceManMove: sink is null, dropping event (%u, %u)"), pixelX, pixelY);
	}
}





#include "Modules/ModuleManager.h"
#include "StarflightBridge.h"
#include "StarflightAssets.h"
#include "Engine/World.h"

#include "GameFramework/PlayerController.h"
#include "StarflightViewportComponent.h"
#include "TimerManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarflightModule, Log, All);

class FStarflightRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Initialize asset system
		FStarflightAssets::Get().Initialize();
		
        // Emulator will be started when HUD BeginPlay() is called (when Play is pressed)
        WorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FStarflightRuntimeModule::OnWorldInit);
	}

    virtual void ShutdownModule() override
    {
        if (WorldInitHandle.IsValid())
		{
			FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitHandle);
			WorldInitHandle.Reset();
		}
		
		// Shutdown asset system
		FStarflightAssets::Get().Shutdown();
		
		// Emulator will be stopped when HUD EndPlay() is called (when Play is stopped)
	}

private:
	FDelegateHandle WorldInitHandle;

	void OnWorldInit(UWorld* World, const UWorld::InitializationValues)
	{
		if (!World) return;
		if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE) return;

        UE_LOG(LogStarflightModule, Warning, TEXT("OnWorldInit for world: %s"), *World->GetName());

        // Input is handled by AStarflightPlayerController during play; no global preprocessor
		
		// Delay spawn until player controller exists
		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(TimerHandle, [World]()
		{
			APlayerController* PC = World->GetFirstPlayerController();
			if (!PC)
			{
				UE_LOG(LogStarflightModule, Warning, TEXT("No PlayerController yet"));
				return;
			}
			
			UE_LOG(LogStarflightModule, Warning, TEXT("Found PlayerController, spawning HUD"));
			
			AHUD* ExistingHUD = PC->GetHUD();
			if (ExistingHUD)
			{
				ExistingHUD->Destroy();
			}
			
			AStarflightHUD* HUD = World->SpawnActor<AStarflightHUD>(AStarflightHUD::StaticClass());
			PC->MyHUD = HUD;
			UE_LOG(LogStarflightModule, Warning, TEXT("Spawned HUD: %p"), HUD);
		}, 0.5f, false);
	}
};

IMPLEMENT_MODULE(FStarflightRuntimeModule, StarflightRuntime)
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "StarflightAssets.h"
#include "Engine/World.h"

#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "Logging/LogMacros.h"
#include "StarflightInputPreprocessor.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"
#include "Templates/SharedPointer.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarflightModule, Log, All);

class FStarflightRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Initialize asset system
		FStarflightAssets::Get().Initialize();
		
		// Map plugin Shaders/ to a virtual path so global shaders can be found
		{
			// Map the project-level Shaders/ (our .usf files are here) to a single virtual path.
			const FString ProjectShaderDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
			AddShaderSourceDirectoryMapping(TEXT("/Starflight"), ProjectShaderDir);
		}

		// Register a global input preprocessor so Tab travel works even when a non-Starflight
		// PlayerController is active (e.g., FirstPerson feature pack controllers).
		TryRegisterInputPreprocessor();

		// Slate may not be initialized yet at module startup depending on engine/editor phase.
		// Defer registration to PostEngineInit to keep module startup safe and predictable.
		if (!InputPreprocessor.IsValid())
		{
			PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(
				this, &FStarflightRuntimeModule::TryRegisterInputPreprocessor);
		}
	}

    virtual void ShutdownModule() override
    {
		if (PostEngineInitHandle.IsValid())
		{
			FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
			PostEngineInitHandle.Reset();
		}

		if (InputPreprocessor.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputPreprocessor);
			InputPreprocessor.Reset();
		}

		// Shutdown asset system
		FStarflightAssets::Get().Shutdown();
	}

private:
	TSharedPtr<FStarflightInputPreprocessor> InputPreprocessor;
	FDelegateHandle PostEngineInitHandle;

	void TryRegisterInputPreprocessor()
	{
		if (InputPreprocessor.IsValid())
		{
			return;
		}

		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		InputPreprocessor = MakeShared<FStarflightInputPreprocessor>();
		FSlateApplication::Get().RegisterInputPreProcessor(InputPreprocessor);

		if (PostEngineInitHandle.IsValid())
		{
			FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
			PostEngineInitHandle.Reset();
		}
	}
};

IMPLEMENT_MODULE(FStarflightRuntimeModule, StarflightRuntime)
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "StarflightAssets.h"
#include "Engine/World.h"

#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "Logging/LogMacros.h"
#include "StarflightInputPreprocessor.h"

#include "Framework/Application/SlateApplication.h"
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
		if (FSlateApplication::IsInitialized())
		{
			InputPreprocessor = MakeShared<FStarflightInputPreprocessor>();
			FSlateApplication::Get().RegisterInputPreProcessor(InputPreprocessor);
		}
	}

    virtual void ShutdownModule() override
    {
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
};

IMPLEMENT_MODULE(FStarflightRuntimeModule, StarflightRuntime)
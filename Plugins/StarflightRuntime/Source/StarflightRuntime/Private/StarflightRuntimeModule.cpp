#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "StarflightAssets.h"
#include "Engine/World.h"

#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "Logging/LogMacros.h"

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
	}

    virtual void ShutdownModule() override
    {
		// Shutdown asset system
		FStarflightAssets::Get().Shutdown();
	}

private:
};

IMPLEMENT_MODULE(FStarflightRuntimeModule, StarflightRuntime)
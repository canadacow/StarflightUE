using UnrealBuildTool;
using System.IO;

public class StarflightUE : ModuleRules
{
    public StarflightUE(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "RenderCore",
            "RHI",
            "StarflightRuntime"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
        });
    }
}



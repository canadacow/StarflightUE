using UnrealBuildTool;
using System.IO;

public class StarflightRuntime : ModuleRules
{
    public StarflightRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDefinitions.Add("NOMINMAX");

        PublicIncludePaths.AddRange(new string[]
        {
            Path.Combine(ModuleDirectory, "Public")
        });

        PrivateIncludePaths.AddRange(new string[]
        {
            Path.Combine(ModuleDirectory, "Private")
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UMG",
            "RenderCore",
            "RHI",
            "Projects",
            "Slate",
            "SlateCore",
            "AudioMixer"
        });
    }
}



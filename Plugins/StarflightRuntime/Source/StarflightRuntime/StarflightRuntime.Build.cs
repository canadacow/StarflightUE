using UnrealBuildTool;
using System.IO;

public class StarflightRuntime : ModuleRules
{
    public StarflightRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        
        // Disable warnings for emulator code
        bEnableExceptions = true;
        bUseRTTI = true;

        PublicDefinitions.Add("NOMINMAX");
        PublicDefinitions.Add("_CRT_SECURE_NO_WARNINGS");
        
        // Disable specific warnings that are treated as errors in emulator code
        bEnableUndefinedIdentifierWarnings = false;
        bWarningsAsErrors = false;

        PublicIncludePaths.AddRange(new string[]
        {
            Path.Combine(ModuleDirectory, "Public"),
            Path.Combine(ModuleDirectory, "Emulator")
        });

        PrivateIncludePaths.AddRange(new string[]
        {
            Path.Combine(ModuleDirectory, "Private"),
            Path.Combine(ModuleDirectory, "Emulator"),
            Path.Combine(ModuleDirectory, "Emulator", "cpu"),
            Path.Combine(ModuleDirectory, "Emulator", "util"),
            Path.Combine(ModuleDirectory, "Emulator", "patch"),
            Path.Combine(ModuleDirectory, "Emulator", "tts")
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Renderer",
            "UMG",
            "ImageWrapper",
            "RenderCore",
            "RHI",
            "Projects",
            "Slate",
            "SlateCore",
            "AudioMixer",
            "InputCore"
        });

        // Disable unity build for emulator files to avoid symbol conflicts
        // These files have static functions (SF_Log) and global state that conflict in unity builds
        bUseUnity = false;
    }
}



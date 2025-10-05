using UnrealBuildTool;
using System.Collections.Generic;

public class StarflightUETarget : TargetRules
{
    public StarflightUETarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        ExtraModuleNames.AddRange(new string[] { "StarflightUE" });
    }
}



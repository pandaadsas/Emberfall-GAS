// Copyright by person HDD  

using UnrealBuildTool;
using System.Collections.Generic;

public class DuraEditorTarget : TargetRules
{
	public DuraEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
	}
}

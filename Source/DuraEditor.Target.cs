// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

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

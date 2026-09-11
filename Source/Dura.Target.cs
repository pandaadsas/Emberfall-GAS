// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

using UnrealBuildTool;
using System.Collections.Generic;

public class DuraTarget : TargetRules
{
	public DuraTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
	}
}

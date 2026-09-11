// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License

using UnrealBuildTool;

public class Dura : ModuleRules
{
	public Dura(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore",
			"EnhancedInput", "GameplayAbilities", "GameplayTags", "UMG", "ModelViewViewModel" });

		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayTasks", "NavigationSystem","Niagara","AIModule" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

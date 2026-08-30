// Copyright by person HDD

using UnrealBuildTool;

public class DuraEditor : ModuleRules
{
	public DuraEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 校验器需要读取 Dura 运行时模块里的类（GA/GE/AbilityInfo/CharacterClassInfo）来判断配置是否合法
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"Dura",
			"EditorSubsystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Slate",
			"SlateCore",
			"InputCore",
			"ToolMenus",
			"WorkspaceMenuStructure",
			"ContentBrowser",
			"AssetRegistry",
			"EditorStyle"
		});
	}
}

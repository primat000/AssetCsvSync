// MIT Licensed. Copyright (c) 2026 Olga Taranova

using UnrealBuildTool;

public class AssetCsvSyncEditorPlugin : ModuleRules
{
	public AssetCsvSyncEditorPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"EditorFramework",
			"PropertyEditor",
			"UnrealEd",
			"AssetRegistry",
			"Json",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"LevelEditor",
			"DesktopPlatform",
		});

	}
}

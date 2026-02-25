// MIT Licensed. Copyright (c) 2026 Olga Taranova

using UnrealBuildTool;

public class SheetTableEditorPlugin : ModuleRules
{
	public SheetTableEditorPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"EditorFramework", 
			"UnrealEd",
			"AssetRegistry",
			"DeveloperSettings",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"LevelEditor",
			"DesktopPlatform",
			"AssetTools"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { 
			"PropertyEditor"
		});
	}
}

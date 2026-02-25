// MIT Licensed. Copyright (c) 2026 Olga Taranova

#include "SheetTableEditorPlugin.h"

#include "SSheetTableEditorPanel.h"

#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

void FSheetTableEditorPluginModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("SheetTableEditorPlugin: Startup"));

	static const FName TabName(TEXT("SheetTableEditor"));
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TabName,
		FOnSpawnTab::CreateRaw(this, &FSheetTableEditorPluginModule::SpawnEditorTab))
		.SetDisplayName(FText::FromString(TEXT("Sheet Table Editor")))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSheetTableEditorPluginModule::RegisterMenus));
}

void FSheetTableEditorPluginModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("SheetTableEditorPlugin: Shutdown"));

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	static const FName TabName(TEXT("SheetTableEditor"));
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

TSharedRef<SDockTab> FSheetTableEditorPluginModule::SpawnEditorTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SSheetTableEditorPanel)
		];
}

void FSheetTableEditorPluginModule::RegisterMenus()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("WindowLayout"));
	Section.AddMenuEntry(
		TEXT("SheetTableEditor.Open"),
		FText::FromString(TEXT("Sheet Table Editor")),
		FText::FromString(TEXT("Open the Sheet Table Editor tab")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("SheetTableEditor")));
		})));
}

IMPLEMENT_MODULE(FSheetTableEditorPluginModule, SheetTableEditorPlugin)

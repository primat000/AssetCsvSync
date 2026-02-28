// MIT Licensed. Copyright (c) 2026 Olga Taranova

#include "AssetCsvSyncEditorPlugin.h"

#include "SAssetCsvSyncEditorPanel.h"
#include "AssetCsvSyncCSVExportSettingsCustomization.h"
#include "AssetCsvSyncCSVImportSettingsCustomization.h"
#include "AssetCsvSyncLog.h"

#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

void FAssetCsvSyncEditorPluginModule::StartupModule()
{
	UE_LOG(LogAssetCsvSync, Log, TEXT("AssetCsvSyncEditorPlugin: Startup"));

	static const FName TabName(TEXT("AssetCsvSync"));
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TabName,
		FOnSpawnTab::CreateRaw(this, &FAssetCsvSyncEditorPluginModule::SpawnEditorTab))
		.SetDisplayName(FText::FromString(TEXT("Asset CSV Sync")))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAssetCsvSyncEditorPluginModule::RegisterMenus));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyEditorModule.RegisterCustomClassLayout(
		TEXT("AssetCsvSyncCSVExportSettings"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FAssetCsvSyncCSVExportSettingsCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomClassLayout(
		TEXT("AssetCsvSyncCSVImportSettings"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FAssetCsvSyncCSVImportSettingsCustomization::MakeInstance));
	PropertyEditorModule.NotifyCustomizationModuleChanged();
}

void FAssetCsvSyncEditorPluginModule::ShutdownModule()
{
	UE_LOG(LogAssetCsvSync, Log, TEXT("AssetCsvSyncEditorPlugin: Shutdown"));

	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditorModule.UnregisterCustomClassLayout(TEXT("AssetCsvSyncCSVExportSettings"));
		PropertyEditorModule.UnregisterCustomClassLayout(TEXT("AssetCsvSyncCSVImportSettings"));
		PropertyEditorModule.NotifyCustomizationModuleChanged();
	}

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	static const FName TabName(TEXT("AssetCsvSync"));
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

TSharedRef<SDockTab> FAssetCsvSyncEditorPluginModule::SpawnEditorTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SAssetCsvSyncEditorPanel)
		];
}

void FAssetCsvSyncEditorPluginModule::RegisterMenus()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("WindowLayout"));
	Section.AddMenuEntry(
		TEXT("AssetCsvSync.Open"),
		FText::FromString(TEXT("Asset CSV Sync")),
		FText::FromString(TEXT("Open the Asset CSV Sync tab")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("AssetCsvSync")));
		})));
}

IMPLEMENT_MODULE(FAssetCsvSyncEditorPluginModule, AssetCsvSyncEditorPlugin)

// MIT Licensed. Copyright (c) 2026 Olga Taranova

#pragma once

#include "CoreMinimal.h"
#include "AssetCsvSyncEditorPanelSettings.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;

class SAssetCsvSyncEditorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetCsvSyncEditorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnExportClicked();
	FReply OnImportClicked();
	void Notify(const FText& Message, bool bSuccess) const;

	TStrongObjectPtr<UAssetCsvSyncCSVExportSettings> ExportSettings;
	TStrongObjectPtr<UAssetCsvSyncCSVImportSettings> ImportSettings;

	TSharedPtr<IDetailsView> ExportDetails;
	TSharedPtr<IDetailsView> ImportDetails;
};

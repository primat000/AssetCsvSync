// MIT Licensed. Copyright (c) 2026 Olga Taranova

#pragma once

#include "CoreMinimal.h"
#include "SheetTableEditorPanelSettings.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;

class SSheetTableEditorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSheetTableEditorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnExportClicked();
	FReply OnImportClicked();
	FReply OnCreateTestAssetClicked();

	FString PromptForSaveCSVPath(const FString& DefaultFileName) const;
	FString PromptForOpenCSVPath() const;
	void Notify(const FText& Message, bool bSuccess) const;

	TStrongObjectPtr<USheetTableCSVExportSettings> ExportSettings;
	TStrongObjectPtr<USheetTableCSVImportSettings> ImportSettings;

	TSharedPtr<IDetailsView> ExportDetails;
	TSharedPtr<IDetailsView> ImportDetails;
};

// MIT Licensed. Copyright (c) 2026 Olga Taranova

#include "SAssetCsvSyncEditorPanel.h"

#include "AssetCsvSyncCSVHandler.h"
#include "AssetCsvSyncEditorPanelSettings.h"
#include "ExportableMetaData.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

// Returns true if the asset's class has a CsvRows TArray property (multi-row mode).
// Handles both TArray<FStruct> and TArray<UObject*> / TArray<TSoftObjectPtr<T>>.
static bool HasCsvRowsAsset(UDataAsset* Asset)
{
	if (!Asset) return false;
	FArrayProperty* Arr = nullptr;
	FStructProperty* Str = nullptr;
	if (FExportableMetaData::FindCsvRowsProperty(Asset->GetClass(), Arr, Str))
		return true;
	UClass* ObjClass = nullptr;
	return FExportableMetaData::FindCsvRowsObjectProperty(Asset->GetClass(), Arr, ObjClass);
}

void SAssetCsvSyncEditorPanel::Construct(const FArguments& InArgs)
{
	ExportSettings = TStrongObjectPtr<UAssetCsvSyncCSVExportSettings>(NewObject<UAssetCsvSyncCSVExportSettings>(GetTransientPackage()));
	ImportSettings = TStrongObjectPtr<UAssetCsvSyncCSVImportSettings>(NewObject<UAssetCsvSyncCSVImportSettings>(GetTransientPackage()));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bLockable = false;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.bAllowSearch = false;

	ExportDetails = PropertyEditorModule.CreateDetailView(DetailsArgs);
	ExportDetails->SetObject(ExportSettings.Get());

	ImportDetails = PropertyEditorModule.CreateDetailView(DetailsArgs);
	ImportDetails->SetObject(ImportSettings.Get());

	ChildSlot
	[
		SNew(SBorder)
		.Padding(10)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Asset CSV Sync")))
				.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
			]

			+ SScrollBox::Slot()
			.Padding(FMargin(0, 10, 0, 10))
			[
				SNew(SSeparator)
			]

			// Export section
			+ SScrollBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Data Asset -> CSV")))
				.Font(FAppStyle::GetFontStyle("HeadingSmall"))
			]

			+ SScrollBox::Slot()
			.Padding(FMargin(0, 6, 0, 6))
			[
				ExportDetails.ToSharedRef()
			]

			+ SScrollBox::Slot()
			.Padding(FMargin(0, 0, 0, 10))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Export to CSV")))
					.IsEnabled_Lambda([this]()
					{
						if (!ExportSettings.IsValid() || !ExportSettings->DataAsset || ExportSettings->CSVFile.FilePath.IsEmpty())
							return false;
						// CsvRows assets export all rows — no column list required.
						if (HasCsvRowsAsset(ExportSettings->DataAsset))
							return true;
						return ExportSettings->ExportColumns.Num() > 0;
					})
					.OnClicked(this, &SAssetCsvSyncEditorPanel::OnExportClicked)
				]
			]

			// Import section
			+ SScrollBox::Slot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("CSV -> Data Asset")))
				.Font(FAppStyle::GetFontStyle("HeadingSmall"))
			]

			+ SScrollBox::Slot()
			.Padding(FMargin(0, 6, 0, 6))
			[
				ImportDetails.ToSharedRef()
			]

			+ SScrollBox::Slot()
			.Padding(FMargin(0, 0, 0, 10))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Import CSV")))
					.IsEnabled_Lambda([this]()
					{
						if (!ImportSettings.IsValid() || !ImportSettings->DataAsset || ImportSettings->CSVFile.FilePath.IsEmpty())
							return false;
						// CsvRows assets import all rows — no column list required.
						if (HasCsvRowsAsset(ImportSettings->DataAsset))
							return true;
						return ImportSettings->ImportColumns.Num() > 0;
					})
					.OnClicked(this, &SAssetCsvSyncEditorPanel::OnImportClicked)
				]
			]
		]
	];
}

FReply SAssetCsvSyncEditorPanel::OnExportClicked()
{
	if (!ExportSettings.IsValid() || !ExportSettings->DataAsset)
	{
		Notify(FText::FromString(TEXT("Select a Data Asset to export.")), false);
		return FReply::Handled();
	}
	if (ExportSettings->CSVFile.FilePath.IsEmpty())
	{
		Notify(FText::FromString(TEXT("Select a CSV file path.")), false);
		return FReply::Handled();
	}

	bool bOk = false;
	if (HasCsvRowsAsset(ExportSettings->DataAsset))
	{
		// Multi-row mode: export all rows from the CsvRows TArray.
		bOk = UAssetCsvSyncCSVHandler::ExportDataAssetRowsToCSV(ExportSettings->DataAsset, ExportSettings->CSVFile.FilePath);
	}
	else
	{
		// Single-asset mode: export selected columns only.
		bOk = UAssetCsvSyncCSVHandler::ExportDataAssetToCSV_Columns(ExportSettings->DataAsset, ExportSettings->CSVFile.FilePath, ExportSettings->ExportColumns);
	}

	Notify(bOk ? FText::FromString(TEXT("Export complete.")) : FText::FromString(TEXT("Export failed. Check Output Log.")), bOk);
	return FReply::Handled();
}

FReply SAssetCsvSyncEditorPanel::OnImportClicked()
{
	if (!ImportSettings.IsValid())
	{
		Notify(FText::FromString(TEXT("Internal error: settings missing.")), false);
		return FReply::Handled();
	}

	const FString SourcePath = ImportSettings->CSVFile.FilePath;
	if (SourcePath.IsEmpty())
	{
		Notify(FText::FromString(TEXT("Select a CSV file path.")), false);
		return FReply::Handled();
	}

	if (!ImportSettings->DataAsset)
	{
		Notify(FText::FromString(TEXT("Select a Data Asset to update.")), false);
		return FReply::Handled();
	}

	bool bOk = false;
	if (HasCsvRowsAsset(ImportSettings->DataAsset))
	{
		// Multi-row mode: import all rows from CSV into the CsvRows TArray.
		// Rows are matched by CsvId if present; otherwise the array is replaced wholesale.
		bOk = UAssetCsvSyncCSVHandler::ImportCSVRowsToDataAsset(SourcePath, ImportSettings->DataAsset, ImportSettings->bSavePackage);
	}
	else
	{
		// Single-asset mode: import selected columns only.
		bOk = UAssetCsvSyncCSVHandler::ImportCSVToDataAssetInPlace(SourcePath, ImportSettings->DataAsset, ImportSettings->ImportColumns, ImportSettings->bSavePackage);
	}

	Notify(bOk ? FText::FromString(TEXT("Import complete.")) : FText::FromString(TEXT("Import failed. Check Output Log.")), bOk);
	return FReply::Handled();
}

void SAssetCsvSyncEditorPanel::Notify(const FText& Message, bool bSuccess) const
{
	FNotificationInfo Info(Message);
	Info.ExpireDuration = 3.0f;
	Info.bUseSuccessFailIcons = true;
	Info.Image = bSuccess ? FAppStyle::GetBrush(TEXT("Icons.Check")) : FAppStyle::GetBrush(TEXT("Icons.Error"));
	FSlateNotificationManager::Get().AddNotification(Info);
}

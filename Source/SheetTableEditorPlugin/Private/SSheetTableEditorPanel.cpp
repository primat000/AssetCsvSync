// MIT Licensed. Copyright (c) 2026 Olga Taranova

#include "SSheetTableEditorPanel.h"

#include "SheetTableCSVHandler.h"
#include "SheetTableEditorPanelSettings.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

void SSheetTableEditorPanel::Construct(const FArguments& InArgs)
{
	ExportSettings = TStrongObjectPtr<USheetTableCSVExportSettings>(NewObject<USheetTableCSVExportSettings>(GetTransientPackage()));
	ImportSettings = TStrongObjectPtr<USheetTableCSVImportSettings>(NewObject<USheetTableCSVImportSettings>(GetTransientPackage()));

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
				.Text(FText::FromString(TEXT("Sheet Table Editor")))
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
				SNew(SButton)
				.Text(FText::FromString(TEXT("Export to CSV")))
				.OnClicked(this, &SSheetTableEditorPanel::OnExportClicked)
			]

			+ SScrollBox::Slot()
			.Padding(FMargin(0, 0, 0, 16))
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Create Test Data Asset")))
				.ToolTipText(FText::FromString(TEXT("Creates /Game/SheetTableTest/DA_SheetTableTest using the C++ test class.")))
				.OnClicked(this, &SSheetTableEditorPanel::OnCreateTestAssetClicked)
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
				SNew(SButton)
				.Text(FText::FromString(TEXT("Import CSV to New Data Asset")))
				.OnClicked(this, &SSheetTableEditorPanel::OnImportClicked)
			]
		]
	];
}
// TODO
FReply SSheetTableEditorPanel::OnExportClicked()
{
	if (!ExportSettings.IsValid() || !ExportSettings->DataAsset)
	{
		Notify(FText::FromString(TEXT("Select a Data Asset to export.")), false);
		return FReply::Handled();
	}

	FString TargetPath = ExportSettings->CSVFile.FilePath;
	if (TargetPath.IsEmpty())
	{
		TargetPath = PromptForSaveCSVPath(ExportSettings->DataAsset->GetName() + TEXT(".csv"));
	}

	if (TargetPath.IsEmpty())
	{
		return FReply::Handled();
	}

	const bool bOk = USheetTableCSVHandler::ExportDataAssetToCSV(ExportSettings->DataAsset, TargetPath);
	Notify(bOk ? FText::FromString(TEXT("Export complete.")) : FText::FromString(TEXT("Export failed. Check Output Log.")), bOk);
	return FReply::Handled();
}
// TODO
FReply SSheetTableEditorPanel::OnImportClicked()
{
	if (!ImportSettings.IsValid())
	{
		Notify(FText::FromString(TEXT("Internal error: settings missing.")), false);
		return FReply::Handled();
	}

	FString SourcePath = ImportSettings->CSVFile.FilePath;
	if (SourcePath.IsEmpty())
	{
		SourcePath = PromptForOpenCSVPath();
	}

	if (SourcePath.IsEmpty())
	{
		return FReply::Handled();
	}

	if (!ImportSettings->DataAssetClass)
	{
		Notify(FText::FromString(TEXT("Select a Data Asset class to import into.")), false);
		return FReply::Handled();
	}

	FString Folder = ImportSettings->TargetFolder;
	if (Folder.IsEmpty())
	{
		Folder = TEXT("/Game");
	}
	if (!Folder.StartsWith(TEXT("/Game")))
	{
		Notify(FText::FromString(TEXT("TargetFolder must start with /Game")), false);
		return FReply::Handled();
	}

	const FString AssetName = ImportSettings->AssetName.IsEmpty() ? TEXT("DA_ImportedFromCSV") : ImportSettings->AssetName;
	const FString AssetPath = Folder / AssetName;

	UDataAsset* Created = nullptr;
	const bool bOk = USheetTableCSVHandler::ImportCSVToNewDataAssetAsset(SourcePath, AssetPath, ImportSettings->DataAssetClass.Get(), Created, ImportSettings->bSavePackage);
	Notify(bOk ? FText::FromString(TEXT("Import complete.")) : FText::FromString(TEXT("Import failed. Check Output Log.")), bOk);
	return FReply::Handled();
}

// TODO
FReply SSheetTableEditorPanel::OnCreateTestAssetClicked()
{
	UClass* TestClass = LoadClass<UDataAsset>(nullptr, TEXT("/Script/SheetTableExport.SheetTableTestDataAsset"));
	if (!TestClass)
	{
		Notify(FText::FromString(TEXT("Test class not found. Build the project and ensure SheetTableTestDataAsset exists.")), false);
		return FReply::Handled();
	}

	UDataAsset* Created = nullptr;
	const bool bOk = USheetTableCSVHandler::CreateNewDataAssetAsset(TEXT("/Game/SheetTableTest/DA_SheetTableTest"), TestClass, Created, true);
	Notify(bOk ? FText::FromString(TEXT("Test Data Asset created.")) : FText::FromString(TEXT("Could not create test asset (maybe it already exists).")), bOk);
	return FReply::Handled();
}

// TODO
FString SSheetTableEditorPanel::PromptForSaveCSVPath(const FString& DefaultFileName) const
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
		return FString();

	TArray<FString> OutFiles;
	const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	const bool bOk = DesktopPlatform->SaveFileDialog(
		ParentWindowHandle,
		TEXT("Export CSV"),
		FPaths::ProjectDir(),
		DefaultFileName,
		TEXT("CSV files (*.csv)|*.csv"),
		EFileDialogFlags::None,
		OutFiles);

	return (bOk && OutFiles.Num() > 0) ? OutFiles[0] : FString();
}

// TODO
FString SSheetTableEditorPanel::PromptForOpenCSVPath() const
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
		return FString();

	TArray<FString> OutFiles;
	const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	const bool bOk = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Import CSV"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("CSV files (*.csv)|*.csv"),
		EFileDialogFlags::None,
		OutFiles);

	return (bOk && OutFiles.Num() > 0) ? OutFiles[0] : FString();
}

// TODO
void SSheetTableEditorPanel::Notify(const FText& Message, bool bSuccess) const
{
	FNotificationInfo Info(Message);
	Info.ExpireDuration = 3.0f;
	Info.bUseSuccessFailIcons = true;
	Info.Image = bSuccess ? FAppStyle::GetBrush(TEXT("Icons.Check")) : FAppStyle::GetBrush(TEXT("Icons.Error"));
	FSlateNotificationManager::Get().AddNotification(Info);
}

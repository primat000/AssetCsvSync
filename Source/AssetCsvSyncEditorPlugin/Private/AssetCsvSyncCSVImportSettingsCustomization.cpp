// MIT Licensed. Copyright (c) 2026 Olga Taranova

#include "AssetCsvSyncCSVImportSettingsCustomization.h"

#include "AssetCsvSyncEditorPanelSettings.h"

#include "DesktopPlatformModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailPropertyRow.h"
#include "Misc/Paths.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IDetailCustomization> FAssetCsvSyncCSVImportSettingsCustomization::MakeInstance()
{
	return MakeShared<FAssetCsvSyncCSVImportSettingsCustomization>();
}

void FAssetCsvSyncCSVImportSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& ImportCategory = DetailBuilder.EditCategory(TEXT("Import"));

	TSharedRef<IPropertyHandle> DataAssetHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetCsvSyncCSVImportSettings, DataAsset));
	TSharedRef<IPropertyHandle> CSVFileHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetCsvSyncCSVImportSettings, CSVFile));
	TSharedPtr<IPropertyHandle> CSVPathHandle = CSVFileHandle->GetChildHandle(TEXT("FilePath"));
	TSharedRef<IPropertyHandle> SaveHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetCsvSyncCSVImportSettings, bSavePackage));

	ImportCategory.AddProperty(DataAssetHandle);

	DetailBuilder.HideProperty(CSVFileHandle);
	ImportCategory.AddCustomRow(FText::FromString(TEXT("CSV File")))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("CSV File")))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(420.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SEditableTextBox)
			.IsReadOnly(true)
			.Text_Lambda([CSVPathHandle]()
			{
				FString Path;
				if (CSVPathHandle.IsValid())
				{
					CSVPathHandle->GetValue(Path);
				}
				return FText::FromString(Path);
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(6, 0, 0, 0))
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Choose...")))
			.OnClicked_Lambda([CSVPathHandle]() -> FReply
			{
				IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
				if (!DesktopPlatform || !CSVPathHandle.IsValid())
					return FReply::Handled();

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

				if (bOk && OutFiles.Num() > 0)
				{
					CSVPathHandle->SetValue(OutFiles[0]);
				}
				return FReply::Handled();
			})
		]
	];

	ImportCategory.AddProperty(SaveHandle);
}

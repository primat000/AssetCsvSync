// MIT Licensed. Copyright (c) 2026 Olga Taranova

#include "AssetCsvSyncCSVExportSettingsCustomization.h"

#include "AssetCsvSyncEditorPanelSettings.h"

#include "DesktopPlatformModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IDetailCustomization> FAssetCsvSyncCSVExportSettingsCustomization::MakeInstance()
{
	return MakeShared<FAssetCsvSyncCSVExportSettingsCustomization>();
}

void FAssetCsvSyncCSVExportSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& ExportCategory = DetailBuilder.EditCategory(TEXT("Export"));

	TSharedRef<IPropertyHandle> DataAssetHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetCsvSyncCSVExportSettings, DataAsset));
	TSharedRef<IPropertyHandle> CSVFileHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetCsvSyncCSVExportSettings, CSVFile));
	TSharedPtr<IPropertyHandle> CSVPathHandle = CSVFileHandle->GetChildHandle(TEXT("FilePath"));

	ExportCategory.AddProperty(DataAssetHandle);

	DetailBuilder.HideProperty(CSVFileHandle);
	ExportCategory.AddCustomRow(FText::FromString(TEXT("CSV File")))
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
			.OnClicked_Lambda([DataAssetHandle, CSVPathHandle]() -> FReply
			{
				IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
				if (!DesktopPlatform || !CSVPathHandle.IsValid())
					return FReply::Handled();

				FString DefaultName = TEXT("Export.csv");
				{
					UObject* Obj = nullptr;
					if (DataAssetHandle->GetValue(Obj) == FPropertyAccess::Success)
					{
						if (UDataAsset* DA = Cast<UDataAsset>(Obj))
						{
							DefaultName = DA->GetName() + TEXT(".csv");
						}
					}
				}

				TArray<FString> OutFiles;
				const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
				const bool bOk = DesktopPlatform->SaveFileDialog(
					ParentWindowHandle,
					TEXT("Export CSV"),
					FPaths::ProjectDir(),
					DefaultName,
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
}

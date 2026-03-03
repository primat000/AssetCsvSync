// MIT Licensed. Copyright (c) 2026 Olga Taranova

#include "AssetCsvSyncCSVImportSettingsCustomization.h"

#include "AssetCsvSyncEditorPanelSettings.h"

#include "DesktopPlatformModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "Misc/Paths.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"

#include "AssetCsvSyncCSVHandler.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Images/SImage.h"

TSharedRef<IDetailCustomization> FAssetCsvSyncCSVImportSettingsCustomization::MakeInstance()
{
	return MakeShared<FAssetCsvSyncCSVImportSettingsCustomization>();
}

void FAssetCsvSyncCSVImportSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& ImportCategory = DetailBuilder.EditCategory(TEXT("Import"));
	TSharedPtr<IPropertyUtilities> PropUtils = DetailBuilder.GetPropertyUtilities();

	TSharedRef<IPropertyHandle> DataAssetHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetCsvSyncCSVImportSettings, DataAsset));
	TSharedRef<IPropertyHandle> CSVFileHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetCsvSyncCSVImportSettings, CSVFile));
	TSharedRef<IPropertyHandle> ColumnsHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetCsvSyncCSVImportSettings, ImportColumns));
	TSharedPtr<IPropertyHandle> CSVPathHandle = CSVFileHandle->GetChildHandle(TEXT("FilePath"));
	TSharedRef<IPropertyHandle> SaveHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetCsvSyncCSVImportSettings, bSavePackage));

	auto PopulateColumns = [ColumnsHandle, PropUtils](const TArray<FString>& Cols)
	{
		if (!PropUtils.IsValid())
			return;
		PropUtils->EnqueueDeferredAction(FSimpleDelegate::CreateLambda([ColumnsHandle, Cols, PropUtils]()
		{
			TSharedPtr<IPropertyHandleArray> ArrayHandle = ColumnsHandle->AsArray();
			if (!ArrayHandle.IsValid())
				return;
			ArrayHandle->EmptyArray();
			for (const FString& Col : Cols)
			{
				ArrayHandle->AddItem();
				uint32 Num = 0;
				ArrayHandle->GetNumElements(Num);
				if (Num == 0)
					continue;
				TSharedRef<IPropertyHandle> Elem = ArrayHandle->GetElement(Num - 1);
				Elem->SetValue(Col);
			}
			PropUtils->RequestForceRefresh();
		}));
	};

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

	// Auto-populate columns when CSV changes
	if (CSVPathHandle.IsValid())
	{
		CSVPathHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([CSVPathHandle, PopulateColumns]()
		{
			FString Path;
			CSVPathHandle->GetValue(Path);
			TArray<FString> Cols;
			if (!Path.IsEmpty())
			{
				UAssetCsvSyncCSVHandler::GetCSVHeaderColumns(Path, Cols);
			}
			PopulateColumns(Cols);
		}));
	}

	// Import Columns chips
	DetailBuilder.HideProperty(ColumnsHandle);
	TSharedPtr<SWrapBox> Chips;
	SAssignNew(Chips, SWrapBox)
		.InnerSlotPadding(FVector2D(6.0f, 6.0f));

		auto RebuildChips = [Chips, ColumnsHandle, PropUtils]()
	{
		if (!Chips.IsValid())
			return;
		Chips->ClearChildren();
		uint32 Num = 0;
		TSharedPtr<IPropertyHandleArray> ArrayHandle = ColumnsHandle->AsArray();
		if (!ArrayHandle.IsValid())
			return;
		ArrayHandle->GetNumElements(Num);
		if (Num == 0)
		{
			Chips->AddSlot()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("No columns selected.")))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
			return;
		}
		for (uint32 Index = 0; Index < Num; ++Index)
		{
			TSharedRef<IPropertyHandle> Elem = ArrayHandle->GetElement(Index);
			FString Name;
			if (Elem->GetValue(Name) != FPropertyAccess::Success)
				continue;

			Chips->AddSlot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SEditableTextBox)
					.IsReadOnly(true)
					.Text(FText::FromString(Name))
					.SelectAllTextWhenFocused(false)
					.RevertTextOnEscape(false)
					.ClearKeyboardFocusOnCommit(false)
					.MinDesiredWidth(180.0f)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(6, 0, 0, 0))
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.ContentPadding(FMargin(4, 2))
					.ToolTipText(FText::FromString(TEXT("Remove column")))
					.OnClicked_Lambda([ColumnsHandle, PropUtils, Index]()
					{
						if (TSharedPtr<IPropertyHandleArray> ArrayHandle = ColumnsHandle->AsArray())
							ArrayHandle->DeleteItem(static_cast<int32>(Index));
						if (PropUtils.IsValid())
							PropUtils->RequestForceRefresh();
						return FReply::Handled();
					})
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.X"))
					]
				]
			];
		}
	};

	RebuildChips();

	ImportCategory.AddCustomRow(FText::FromString(TEXT("Import Columns")))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Import Columns")))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(420.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SBox)
			.MaxDesiredHeight(280.0f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)
				+ SScrollBox::Slot()
				[
					Chips.ToSharedRef()
				]
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(8, 0, 0, 0))
		.VAlign(VAlign_Top)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Reset")))
			.ToolTipText(FText::FromString(TEXT("Restore the full set of columns from the CSV header.")))
			.OnClicked_Lambda([CSVPathHandle, PopulateColumns]()
			{
				FString Path;
				if (CSVPathHandle.IsValid())
					CSVPathHandle->GetValue(Path);
				TArray<FString> Cols;
				if (!Path.IsEmpty())
					UAssetCsvSyncCSVHandler::GetCSVHeaderColumns(Path, Cols);
				PopulateColumns(Cols);
				return FReply::Handled();
			})
		]
	];

	ImportCategory.AddProperty(DataAssetHandle);
	ImportCategory.AddProperty(SaveHandle);
}

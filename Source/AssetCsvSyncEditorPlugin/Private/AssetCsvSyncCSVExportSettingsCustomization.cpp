// MIT Licensed. Copyright (c) 2026 Olga Taranova

#include "AssetCsvSyncCSVExportSettingsCustomization.h"

#include "AssetCsvSyncEditorPanelSettings.h"

#include "DesktopPlatformModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "Framework/Application/SlateApplication.h"
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
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Images/SImage.h"

#include "AssetCsvSyncCSVHandler.h"

TSharedRef<IDetailCustomization> FAssetCsvSyncCSVExportSettingsCustomization::MakeInstance()
{
	return MakeShared<FAssetCsvSyncCSVExportSettingsCustomization>();
}

void FAssetCsvSyncCSVExportSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& ExportCategory = DetailBuilder.EditCategory(TEXT("Export"));
	TSharedPtr<IPropertyUtilities> PropUtils = DetailBuilder.GetPropertyUtilities();

	TSharedRef<IPropertyHandle> DataAssetHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetCsvSyncCSVExportSettings, DataAsset));
	TSharedRef<IPropertyHandle> ColumnsHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetCsvSyncCSVExportSettings, ExportColumns));
	TSharedRef<IPropertyHandle> CSVFileHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAssetCsvSyncCSVExportSettings, CSVFile));
	TSharedPtr<IPropertyHandle> CSVPathHandle = CSVFileHandle->GetChildHandle(TEXT("FilePath"));

	ExportCategory.AddProperty(DataAssetHandle);

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

	// Auto-populate columns when asset changes
	DataAssetHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([DataAssetHandle, PopulateColumns]()
	{
		UObject* Obj = nullptr;
		if (DataAssetHandle->GetValue(Obj) != FPropertyAccess::Success)
			Obj = nullptr;
		UDataAsset* DA = Cast<UDataAsset>(Obj);
		TArray<FString> Cols;
		if (DA)
		{
			Cols = UAssetCsvSyncCSVHandler::GetExportableProperties(DA->GetClass());
		}
		PopulateColumns(Cols);
	}));

	// Export Columns (chips)
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

	ExportCategory.AddCustomRow(FText::FromString(TEXT("Export Columns")))
		.NameContent()
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("Export Columns")))
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
						.ToolTipText(FText::FromString(TEXT("Restore the full set of exportable columns for the selected asset.")))
						.OnClicked_Lambda([DataAssetHandle, PopulateColumns]()
							{
								UObject* Obj = nullptr;
								if (DataAssetHandle->GetValue(Obj) != FPropertyAccess::Success)
									Obj = nullptr;
								UDataAsset* DA = Cast<UDataAsset>(Obj);
								TArray<FString> Cols;
								if (DA)
									Cols = UAssetCsvSyncCSVHandler::GetExportableProperties(DA->GetClass());
								PopulateColumns(Cols);
								return FReply::Handled();
							})
				]
		];

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

	RebuildChips();
}

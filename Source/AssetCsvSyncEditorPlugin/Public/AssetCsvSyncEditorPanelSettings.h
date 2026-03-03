// MIT Licensed. Copyright (c) 2026 Olga Taranova

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/Object.h"
#include "UObject/NoExportTypes.h"
#include "AssetCsvSyncEditorPanelSettings.generated.h"

UCLASS()
class ASSETCSVSYNCEDITORPLUGIN_API UAssetCsvSyncCSVExportSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Export")
	TObjectPtr<UDataAsset> DataAsset = nullptr;

	UPROPERTY(EditAnywhere, Category = "Export")
	FFilePath CSVFile;

	// Populated automatically from the selected asset; remove items to exclude columns.
	UPROPERTY(EditAnywhere, Category = "Export")
	TArray<FString> ExportColumns;
};

UCLASS()
class ASSETCSVSYNCEDITORPLUGIN_API UAssetCsvSyncCSVImportSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Import")
	TObjectPtr<UDataAsset> DataAsset = nullptr;

	UPROPERTY(EditAnywhere, Category = "Import")
	FFilePath CSVFile;

	// Populated automatically from the CSV header; remove items to exclude columns.
	UPROPERTY(EditAnywhere, Category = "Import")
	TArray<FString> ImportColumns;

	UPROPERTY(EditAnywhere, Category = "Import")
	bool bSavePackage = true;
};

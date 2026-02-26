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

	UPROPERTY(EditAnywhere, Category = "Import")
	bool bSavePackage = true;
};

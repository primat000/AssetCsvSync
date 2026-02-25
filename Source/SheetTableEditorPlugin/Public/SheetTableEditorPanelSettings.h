// MIT Licensed. Copyright (c) 2026 Olga Taranova

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/Object.h"
#include "UObject/NoExportTypes.h"
#include "SheetTableEditorPanelSettings.generated.h"

UCLASS()
class SHEETTABLEEDITORPLUGIN_API USheetTableCSVExportSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Export")
	TObjectPtr<UDataAsset> DataAsset = nullptr;

	UPROPERTY(EditAnywhere, Category = "Export", meta = (FilePathFilter = "CSV files (*.csv)|*.csv"))
	FFilePath CSVFile;
};

UCLASS()
class SHEETTABLEEDITORPLUGIN_API USheetTableCSVImportSettings : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Import", meta = (FilePathFilter = "CSV files (*.csv)|*.csv"))
	FFilePath CSVFile;

	UPROPERTY(EditAnywhere, Category = "Import", meta = (AllowAbstract = "false"))
	TSubclassOf<UDataAsset> DataAssetClass;

	UPROPERTY(EditAnywhere, Category = "Import")
	FString TargetFolder = TEXT("/Game/SheetTableTest");

	UPROPERTY(EditAnywhere, Category = "Import")
	FString AssetName = TEXT("DA_ImportedFromCSV");

	UPROPERTY(EditAnywhere, Category = "Import")
	bool bSavePackage = true;
};

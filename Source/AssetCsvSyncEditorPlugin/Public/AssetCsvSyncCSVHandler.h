// MIT Licensed. Copyright (c) 2026 Olga Taranova

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "ExportableMetaData.h"
#include "AssetCsvSyncCSVHandler.generated.h"

UCLASS()
class ASSETCSVSYNCEDITORPLUGIN_API UAssetCsvSyncCSVHandler : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AssetCsvSync")
	static bool ExportDataAssetToCSV(UDataAsset* DataAsset, const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "AssetCsvSync")
	static bool ImportCSVToDataAsset(const FString& FilePath, UDataAsset*& OutDataAsset, UClass* DataAssetClass);

	// Creates a new DataAsset at AssetPath and imports the CSV into it
	UFUNCTION(BlueprintCallable, Category = "AssetCsvSync")
	static bool ImportCSVToNewDataAssetAsset(const FString& FilePath, const FString& AssetPath, UClass* DataAssetClass, UDataAsset*& OutDataAsset, bool bSavePackage);

	// Creates a new DataAsset asset at AssetPath
	UFUNCTION(BlueprintCallable, Category = "AssetCsvSync")
	static bool CreateNewDataAssetAsset(const FString& AssetPath, UClass* DataAssetClass, UDataAsset*& OutDataAsset, bool bSavePackage);

	UFUNCTION(BlueprintCallable, Category = "AssetCsvSync")
	static TArray<FString> GetExportableDataAssetClasses();

	UFUNCTION(BlueprintCallable, Category = "AssetCsvSync")
	static TArray<FString> GetExportableProperties(UClass* Class);

private:
	static bool CanExportClass(UClass* Class);
	static bool ApplyCSVRowToObject(UObject* TargetObject, UClass* TargetClass, const TArray<FString>& Headers, const TArray<FString>& Values);
	static bool ApplyColumnsToObject(UObject* TargetObject, UClass* TargetClass, const TMap<FString, FString>& ColumnToValue, const FString& Prefix, TSet<const UObject*>& Visited);
	static void ExportObjectToColumns(UObject* ObjectOrNull, UClass* Class, TMap<FString, FString>& InOutColumnToValue, TArray<FString>& InOutColumnOrder, const FString& Prefix, TSet<const UObject*>& Visited);
	static void ExportClassColumnsEmpty(UClass* Class, TMap<FString, FString>& InOutColumnToValue, TArray<FString>& InOutColumnOrder, const FString& Prefix);
	static void ExportStructToColumns(const void* StructPtr, UScriptStruct* Struct, TMap<FString, FString>& InOutColumnToValue, TArray<FString>& InOutColumnOrder, const FString& Prefix, TSet<const UObject*>& Visited);
	static UObject* ResolveObjectPropertyValue(UObject* Container, FProperty* Property, bool bLoadSoft);
	static UObject* ResolveObjectPropertyValueFromContainerPtr(const void* ContainerPtr, FProperty* Property, bool bLoadSoft);
	static UClass* GetObjectPropertyClass(FProperty* Property);
	static bool ApplyColumnsToStruct(void* StructPtr, UScriptStruct* Struct, const TMap<FString, FString>& ColumnToValue, const FString& Prefix, TSet<const UObject*>& Visited);

	static FString PropertyToString(FProperty* Property, const uint8* PropertyData);
	static bool StringToProperty(FProperty* Property, uint8* PropertyData, const FString& StringValue);
	static FString EscapeCSVString(const FString& Value);
	static TArray<FString> ParseCSVLine(const FString& Line);
	static FString EscapeListItem(const FString& Value);
	static TArray<FString> ParseListCell(const FString& Cell);
	static FString JoinListCell(const TArray<FString>& Items);
	static bool SplitAssetPath(const FString& InAssetPath, FString& OutPackageName, FString& OutAssetName);
	static bool SaveCreatedAsset(UPackage* Package, UObject* AssetObject);
};

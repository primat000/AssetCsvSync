// MIT Licensed. Copyright (c) 2026 Olga Taranova

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "ExportableMetaData.h"
#include "SheetTableCSVHandler.generated.h"

UCLASS()
class SHEETTABLEEDITORPLUGIN_API USheetTableCSVHandler : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SheetTableExport")
	static bool ExportDataAssetToCSV(UDataAsset* DataAsset, const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "SheetTableExport")
	static bool ImportCSVToDataAsset(const FString& FilePath, UDataAsset*& OutDataAsset, UClass* DataAssetClass);

	// Creates a new DataAsset at AssetPath (e.g. /Game/Folder/DA_Name) and imports the CSV into it.
	UFUNCTION(BlueprintCallable, Category = "SheetTableExport")
	static bool ImportCSVToNewDataAssetAsset(const FString& FilePath, const FString& AssetPath, UClass* DataAssetClass, UDataAsset*& OutDataAsset, bool bSavePackage);

	// Creates a new DataAsset asset at AssetPath (e.g. /Game/Folder/DA_Name).
	UFUNCTION(BlueprintCallable, Category = "SheetTableExport")
	static bool CreateNewDataAssetAsset(const FString& AssetPath, UClass* DataAssetClass, UDataAsset*& OutDataAsset, bool bSavePackage);

	UFUNCTION(BlueprintCallable, Category = "SheetTableExport")
	static TArray<FString> GetExportableDataAssetClasses();

	UFUNCTION(BlueprintCallable, Category = "SheetTableExport")
	static TArray<FString> GetExportableProperties(UClass* Class);

private:
	struct FExportColumnInfo
	{
		FString ColumnName;
		TArray<FProperty*> PropertyChain;
	};

	static bool CanExportClass(UClass* Class);
	static void GetExportablePropertiesRecursively(UClass* Class, TArray<FExportColumnInfo>& OutColumns);
	static void BuildColumnsForClass(UClass* Class, TArray<FExportColumnInfo>& OutColumns, TArray<UClass*>& ClassStack, const TArray<FProperty*>& PrefixChain, const FString& ColumnPrefix);
	static UObject* ResolveObjectPropertyValue(UObject* Container, FProperty* Property, bool bLoadSoft);
	static UClass* GetObjectPropertyClass(FProperty* Property);
	static bool GetColumnValue(UObject* RootObject, const FExportColumnInfo& Column, FString& OutValue);
	static bool SetColumnValue(UObject* RootObject, const FExportColumnInfo& Column, const FString& InValue);
	static bool ApplyCSVRowToObject(UObject* TargetObject, UClass* TargetClass, const TArray<FString>& Headers, const TArray<FString>& Values);
	static FString PropertyToString(FProperty* Property, const uint8* PropertyData);
	static bool StringToProperty(FProperty* Property, uint8* PropertyData, const FString& StringValue);
	static FString EscapeCSVString(const FString& Value);
	static TArray<FString> ParseCSVLine(const FString& Line);
	static bool SplitAssetPath(const FString& InAssetPath, FString& OutPackageName, FString& OutAssetName);
	static bool SaveCreatedAsset(UPackage* Package, UObject* AssetObject);
};

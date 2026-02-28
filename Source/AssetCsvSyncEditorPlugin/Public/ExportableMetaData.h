// MIT Licensed. Copyright (c) 2026 Olga Taranova

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

class ASSETCSVSYNCEDITORPLUGIN_API FExportableMetaData
{
public:
	static FName GetExportableMetaDataKey()
	{
		// Class-level flag for CSV export/import
		return FName(TEXT("CsvExport"));
	}

	static FName GetCsvColumnMetaDataKey()
	{
		// Property-level column name
		return FName(TEXT("CsvColumn"));
	}

	static FName GetCsvExpandMetaDataKey()
	{
		// Property-level "expand nested fields" flag
		return FName(TEXT("CsvExpand"));
	}

	static bool IsExportable(UClass* Class)
	{
		if (!Class)
			return false;

		return Class->HasMetaData(GetExportableMetaDataKey());
	}

	static bool IsExportable(UFunction* Function)
	{
		if (!Function)
			return false;

		return Function->HasMetaData(GetExportableMetaDataKey());
	}

	static bool IsExportable(FProperty* Property)
	{
		if (!Property)
			return false;
		return HasCsvColumn(Property) || HasCsvExpand(Property);
	}

	static bool HasCsvColumn(FProperty* Property)
	{
		return Property && Property->HasMetaData(GetCsvColumnMetaDataKey());
	}

	static FString GetCsvColumn(FProperty* Property)
	{
		if (!Property)
			return FString();

		return Property->GetMetaData(GetCsvColumnMetaDataKey());
	}

	static bool HasCsvExpand(FProperty* Property)
	{
		return Property && Property->HasMetaData(GetCsvExpandMetaDataKey());
	}
};

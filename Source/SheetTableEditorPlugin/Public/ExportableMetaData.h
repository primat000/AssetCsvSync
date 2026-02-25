// MIT Licensed. Copyright (c) 2026 Olga Taranova

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

class SHEETTABLEEDITORPLUGIN_API FExportableMetaData
{
public:
	static FName GetExportableMetaDataKey()
	{
		// Class-level opt-in for CSV export/import
		return FName(TEXT("CsvExport"));
	}

	static FName GetCsvColumnMetaDataKey()
	{
		// Property-level column name
		return FName(TEXT("CsvColumn"));
	}

	static FName GetCsvExpandMetaDataKey()
	{
		// Property-level "expand nested fields" marker
		return FName(TEXT("CsvExpand"));
	}

	static FName GetCsvPrefixMetaDataKey()
	{
		// Property-level prefix for expanded nested columns
		return FName(TEXT("CsvPrefix"));
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

	static FString GetCsvPrefix(FProperty* Property)
	{
		if (!Property)
			return FString();

		return Property->GetMetaData(GetCsvPrefixMetaDataKey());
	}
};

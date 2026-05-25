// MIT Licensed. Copyright (c) 2026 Olga Taranova

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyIterator.h"

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

	static bool HasCsvRows(FProperty* Property)
	{
		return Property && Property->HasMetaData(TEXT("CsvRows"));
	}

	static bool HasCsvId(FProperty* Property)
	{
		return Property && Property->HasMetaData(TEXT("CsvId"));
	}

	// Returns the value of CsvExport = "..." on the class, or empty string if the tag
	// has no value or the class is null.  Used as the default CSV filename suggestion.
	static FString GetCsvExportName(UClass* Class)
	{
		if (!Class) return FString();
		return Class->GetMetaData(GetExportableMetaDataKey());
	}

	// Finds the first TArray<FStruct> property tagged CsvRows on the class.
	// Returns true and fills OutArrayProp / OutStructProp on success.
	static bool FindCsvRowsProperty(UClass* Class, FArrayProperty*& OutArrayProp, FStructProperty*& OutStructProp)
	{
		if (!Class) return false;
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			FProperty* Prop = *It;
			if (!HasCsvRows(Prop)) continue;
			FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
			if (!ArrayProp) continue;
			FStructProperty* StructProp = CastField<FStructProperty>(ArrayProp->Inner);
			if (!StructProp) continue;
			OutArrayProp  = ArrayProp;
			OutStructProp = StructProp;
			return true;
		}
		return false;
	}

	// Finds the first TArray<UObject*> or TArray<TSoftObjectPtr<T>> property tagged CsvRows.
	// Returns true and fills OutArrayProp / OutElementClass on success.
	// Struct arrays (handled by FindCsvRowsProperty) are intentionally skipped.
	static bool FindCsvRowsObjectProperty(UClass* Class, FArrayProperty*& OutArrayProp, UClass*& OutElementClass)
	{
		if (!Class) return false;
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			FProperty* Prop = *It;
			if (!HasCsvRows(Prop)) continue;
			FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
			if (!ArrayProp) continue;
			if (CastField<FStructProperty>(ArrayProp->Inner)) continue; // struct — handled elsewhere
			if (FObjectProperty* ObjProp = CastField<FObjectProperty>(ArrayProp->Inner))
			{
				OutArrayProp   = ArrayProp;
				OutElementClass = ObjProp->PropertyClass;
				return true;
			}
			if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(ArrayProp->Inner))
			{
				OutArrayProp   = ArrayProp;
				OutElementClass = SoftProp->PropertyClass;
				return true;
			}
		}
		return false;
	}
};

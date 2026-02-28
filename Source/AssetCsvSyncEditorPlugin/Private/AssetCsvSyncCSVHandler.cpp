// MIT Licensed. Copyright (c) 2026 Olga Taranova

#include "AssetCsvSyncCSVHandler.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SavePackage.h"

#include "AssetCsvSyncEditorPluginSettings.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// TODO: check for nested structures, containers, wrong meta tags usage

bool UAssetCsvSyncCSVHandler::ExportDataAssetToCSV(UDataAsset* DataAsset, const FString& FilePath)
{
	if (!DataAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("ExportDataAssetToCSV: DataAsset is null"));
		return false;
	}

	UClass* Class = DataAsset->GetClass();
	if (!CanExportClass(Class))
	{
		UE_LOG(LogTemp, Error, TEXT("ExportDataAssetToCSV: Class %s is not marked with meta=(CsvExport)"), *Class->GetName());
		return false;
	}

	TMap<FString, FString> ColumnToValue;
	TArray<FString> ColumnOrder;
	TSet<const UObject*> Visited;
	ExportObjectToColumns(DataAsset, Class, ColumnToValue, ColumnOrder, FString(), Visited);

	if (ColumnOrder.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ExportDataAssetToCSV: No exportable properties found for class %s"), *Class->GetName());
		return false;
	}

	FString CSVContent;
	for (const FString& ColName : ColumnOrder)
	{
		CSVContent += EscapeCSVString(ColName) + TEXT(",");
	}
	CSVContent.RemoveFromEnd(TEXT(","));
	CSVContent += TEXT("\n");

	for (const FString& ColName : ColumnOrder)
	{
		const FString* Value = ColumnToValue.Find(ColName);
		CSVContent += EscapeCSVString(Value ? *Value : FString()) + TEXT(",");
	}
	CSVContent.RemoveFromEnd(TEXT(","));
	CSVContent += TEXT("\n");

	return FFileHelper::SaveStringToFile(CSVContent, *FilePath);
}

bool UAssetCsvSyncCSVHandler::ImportCSVToDataAsset(const FString& FilePath, UDataAsset*& OutDataAsset, UClass* DataAssetClass)
{
	if (!CanExportClass(DataAssetClass))
	{
		FString ClassName = DataAssetClass ? DataAssetClass->GetName() : TEXT("null");
		UE_LOG(LogTemp, Error, TEXT("ImportCSVToDataAsset: Class %s is not marked with meta=(CsvExport)"), *ClassName);
		return false;
	}

	FString CSVContent;
	if (!FFileHelper::LoadFileToString(CSVContent, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("ImportCSVToDataAsset: Could not load file %s"), *FilePath);
		return false;
	}

	TArray<FString> Lines;
	CSVContent.ParseIntoArrayLines(Lines, false);

	if (Lines.Num() < 2)
	{
		UE_LOG(LogTemp, Error, TEXT("ImportCSVToDataAsset: CSV file has insufficient data"));
		return false;
	}

	TArray<FString> ColumnHeaders = ParseCSVLine(Lines[0]);
	TArray<FString> Values = ParseCSVLine(Lines[1]);

	if (ColumnHeaders.Num() != Values.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("ImportCSVToDataAsset: Column count mismatch"));
		return false;
	}

	UObject* CreatedObject = NewObject<UObject>(GetTransientPackage(), DataAssetClass);
	UDataAsset* NewDataAsset = Cast<UDataAsset>(CreatedObject);

	if (!NewDataAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("ImportCSVToDataAsset: Could not create DataAsset"));
		return false;
	}

	if (!ApplyCSVRowToObject(NewDataAsset, DataAssetClass, ColumnHeaders, Values))
	{
		return false;
	}

	OutDataAsset = NewDataAsset;
	return true;
}

bool UAssetCsvSyncCSVHandler::ImportCSVToNewDataAssetAsset(const FString& FilePath, const FString& AssetPath, UClass* DataAssetClass, UDataAsset*& OutDataAsset, bool bSavePackage)
{
	OutDataAsset = nullptr;

	if (!DataAssetClass || !DataAssetClass->IsChildOf(UDataAsset::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("ImportCSVToNewDataAssetAsset: invalid class"));
		return false;
	}
	if (!CanExportClass(DataAssetClass))
	{
		UE_LOG(LogTemp, Error, TEXT("ImportCSVToNewDataAssetAsset: Class %s is not marked with meta=(CsvExport)"), *DataAssetClass->GetName());
		return false;
	}

	FString PackageName;
	FString AssetName;
	if (!SplitAssetPath(AssetPath, PackageName, AssetName))
	{
		UE_LOG(LogTemp, Error, TEXT("ImportCSVToNewDataAssetAsset: invalid AssetPath %s"), *AssetPath);
		return false;
	}
	const FString ObjectPath = PackageName + TEXT(".") + AssetName;

	// If an asset already exists at the path, update it in-place.
	if (UObject* ExistingObj = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath))
	{
		UDataAsset* ExistingAsset = Cast<UDataAsset>(ExistingObj);
		if (!ExistingAsset)
		{
			UE_LOG(LogTemp, Error, TEXT("ImportCSVToNewDataAssetAsset: existing object is not a DataAsset: %s"), *ObjectPath);
			return false;
		}
		if (!ExistingAsset->IsA(DataAssetClass))
		{
			UE_LOG(LogTemp, Error, TEXT("ImportCSVToNewDataAssetAsset: existing asset class mismatch. Existing=%s Expected=%s (%s)"), *ExistingAsset->GetClass()->GetName(), *DataAssetClass->GetName(), *ObjectPath);
			return false;
		}

		FString CSVContent;
		if (!FFileHelper::LoadFileToString(CSVContent, *FilePath))
		{
			UE_LOG(LogTemp, Error, TEXT("ImportCSVToNewDataAssetAsset: Could not load file %s"), *FilePath);
			return false;
		}
		TArray<FString> Lines;
		CSVContent.ParseIntoArrayLines(Lines, false);
		if (Lines.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("ImportCSVToNewDataAssetAsset: CSV file has insufficient data"));
			return false;
		}
		TArray<FString> ColumnHeaders = ParseCSVLine(Lines[0]);
		TArray<FString> Values = ParseCSVLine(Lines[1]);
		if (ColumnHeaders.Num() != Values.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("ImportCSVToNewDataAssetAsset: Column count mismatch"));
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("ImportCSVToNewDataAssetAsset: Updating existing asset %s"), *ObjectPath);
		if (!ApplyCSVRowToObject(ExistingAsset, ExistingAsset->GetClass(), ColumnHeaders, Values))
		{
			return false;
		}

		ExistingAsset->MarkPackageDirty();
		ExistingAsset->PostEditChange();
		if (bSavePackage)
		{
			SaveCreatedAsset(ExistingAsset->GetPackage(), ExistingAsset);
		}
		OutDataAsset = ExistingAsset;
		return true;
	}

	UDataAsset* NewAsset = nullptr;
	if (!CreateNewDataAssetAsset(AssetPath, DataAssetClass, NewAsset, false))
	{
		return false;
	}

	FString CSVContent;
	if (!FFileHelper::LoadFileToString(CSVContent, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("ImportCSVToNewDataAssetAsset: Could not load file %s"), *FilePath);
		return false;
	}

	TArray<FString> Lines;
	CSVContent.ParseIntoArrayLines(Lines, false);
	if (Lines.Num() < 2)
	{
		UE_LOG(LogTemp, Error, TEXT("ImportCSVToNewDataAssetAsset: CSV file has insufficient data"));
		return false;
	}

	TArray<FString> ColumnHeaders = ParseCSVLine(Lines[0]);
	TArray<FString> Values = ParseCSVLine(Lines[1]);
	if (ColumnHeaders.Num() != Values.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("ImportCSVToNewDataAssetAsset: Column count mismatch"));
		return false;
	}

	if (!ApplyCSVRowToObject(NewAsset, DataAssetClass, ColumnHeaders, Values))
	{
		return false;
	}

	NewAsset->MarkPackageDirty();
	NewAsset->PostEditChange();

	if (bSavePackage)
	{
		SaveCreatedAsset(NewAsset->GetPackage(), NewAsset);
	}

	OutDataAsset = NewAsset;
	return true;
}

bool UAssetCsvSyncCSVHandler::CreateNewDataAssetAsset(const FString& AssetPath, UClass* DataAssetClass, UDataAsset*& OutDataAsset, bool bSavePackage)
{
	OutDataAsset = nullptr;

	if (!DataAssetClass || !DataAssetClass->IsChildOf(UDataAsset::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNewDataAssetAsset: invalid class"));
		return false;
	}

	if (!CanExportClass(DataAssetClass))
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNewDataAssetAsset: Class %s is not marked with meta=(CsvExport)"), *DataAssetClass->GetName());
		return false;
	}

	FString PackageName;
	FString AssetName;
	if (!SplitAssetPath(AssetPath, PackageName, AssetName))
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNewDataAssetAsset: invalid AssetPath %s"), *AssetPath);
		return false;
	}

	{
		const FString ObjectPath = PackageName + TEXT(".") + AssetName;
		FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		const FAssetData Existing = AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
		if (Existing.IsValid() || FindObject<UObject>(nullptr, *ObjectPath))
		{
			UE_LOG(LogTemp, Warning, TEXT("CreateNewDataAssetAsset: asset already exists %s"), *ObjectPath);
			return false;
		}
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNewDataAssetAsset: failed to create package %s"), *PackageName);
		return false;
	}

	UDataAsset* NewDataAsset = NewObject<UDataAsset>(Package, DataAssetClass, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!NewDataAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateNewDataAssetAsset: failed to create asset object"));
		return false;
	}

	FAssetRegistryModule::AssetCreated(NewDataAsset);
	NewDataAsset->MarkPackageDirty();

	if (bSavePackage)
	{
		SaveCreatedAsset(Package, NewDataAsset);
	}

	OutDataAsset = NewDataAsset;
	return true;
}

TArray<FString> UAssetCsvSyncCSVHandler::GetExportableDataAssetClasses()
{
	TArray<FString> Result;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class)
			continue;
		if (!Class->IsChildOf(UDataAsset::StaticClass()))
			continue;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			continue;
		if (CanExportClass(Class))
		{
			Result.Add(Class->GetPathName());
		}
	}
	Result.Sort();
	return Result;
}

bool UAssetCsvSyncCSVHandler::SplitAssetPath(const FString& InAssetPath, FString& OutPackageName, FString& OutAssetName)
{
	FString Path = InAssetPath;
	Path.TrimStartAndEndInline();

	if (Path.IsEmpty() || !Path.StartsWith(TEXT("/Game")))
		return false;

	if (Path.Contains(TEXT(".")))
	{
		FString Left, Right;
		if (!Path.Split(TEXT("."), &Left, &Right) || Left.IsEmpty() || Right.IsEmpty())
			return false;
		OutPackageName = Left;
		OutAssetName = Right;
	}
	else
	{
		OutPackageName = Path;
		OutAssetName = FPackageName::GetShortName(Path);
	}

	if (!FPackageName::IsValidLongPackageName(OutPackageName))
		return false;
	if (!FName::IsValidXName(OutAssetName, INVALID_OBJECTNAME_CHARACTERS))
		return false;

	return true;
}

bool UAssetCsvSyncCSVHandler::SaveCreatedAsset(UPackage* Package, UObject* AssetObject)
{
	if (!Package || !AssetObject)
		return false;

	const FString PackageName = Package->GetName();
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_None;
	SaveArgs.Error = GError;

	const bool bOk = UPackage::SavePackage(Package, AssetObject, *Filename, SaveArgs);
	if (!bOk)
	{
		UE_LOG(LogTemp, Error, TEXT("SavePackage failed: %s"), *Filename);
	}
	return bOk;
}

TArray<FString> UAssetCsvSyncCSVHandler::GetExportableProperties(UClass* Class)
{
	TMap<FString, FString> ColumnToValue;
	TArray<FString> ColumnOrder;
	ExportClassColumnsEmpty(Class, ColumnToValue, ColumnOrder, FString());
	return ColumnOrder;
}

bool UAssetCsvSyncCSVHandler::CanExportClass(UClass* Class)
{
	if (!Class)
		return false;
	return FExportableMetaData::IsExportable(Class);
}

void UAssetCsvSyncCSVHandler::ExportClassColumnsEmpty(UClass* Class, TMap<FString, FString>& InOutColumnToValue, TArray<FString>& InOutColumnOrder, const FString& Prefix)
{
	if (!Class)
		return;

	auto AddColumn = [&InOutColumnToValue, &InOutColumnOrder](const FString& Name, const FString& Value)
	{
		if (!InOutColumnToValue.Contains(Name))
		{
			InOutColumnOrder.Add(Name);
		}
		InOutColumnToValue.Add(Name, Value);
	};

	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
			continue;

		if (FExportableMetaData::HasCsvColumn(Property))
		{
			const FString ColumnKey = FExportableMetaData::GetCsvColumn(Property);
			const FString ColumnName = Prefix + (!ColumnKey.IsEmpty() ? ColumnKey : Property->GetName());
			AddColumn(ColumnName, FString());
		}

		if (FExportableMetaData::HasCsvExpand(Property))
		{
			// Containers need runtime data; only object expansion is representable from class alone.
			if (CastField<FSetProperty>(Property) || CastField<FArrayProperty>(Property) || CastField<FMapProperty>(Property))
			{
				continue;
			}

			UClass* InnerClass = GetObjectPropertyClass(Property);
			if (!InnerClass || !CanExportClass(InnerClass))
				continue;

			const FString ExpandPrefix = Property->GetName() + TEXT("_");

			ExportClassColumnsEmpty(InnerClass, InOutColumnToValue, InOutColumnOrder, Prefix + ExpandPrefix);
		}
	}

	if (UClass* SuperClass = Class->GetSuperClass())
	{
		ExportClassColumnsEmpty(SuperClass, InOutColumnToValue, InOutColumnOrder, Prefix);
	}
}

void UAssetCsvSyncCSVHandler::ExportObjectToColumns(UObject* ObjectOrNull, UClass* Class, TMap<FString, FString>& InOutColumnToValue, TArray<FString>& InOutColumnOrder, const FString& Prefix, TSet<const UObject*>& Visited)
{
	if (!Class)
		return;

	if (ObjectOrNull && Visited.Contains(ObjectOrNull))
		return;
	if (ObjectOrNull)
		Visited.Add(ObjectOrNull);

	auto AddColumn = [&InOutColumnToValue, &InOutColumnOrder](const FString& Name, const FString& Value)
	{
		if (!InOutColumnToValue.Contains(Name))
		{
			InOutColumnOrder.Add(Name);
		}
		InOutColumnToValue.Add(Name, Value);
	};

	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
			continue;

		if (FExportableMetaData::HasCsvColumn(Property))
		{
			const FString ColumnKey = FExportableMetaData::GetCsvColumn(Property);
			const FString ColumnName = Prefix + (!ColumnKey.IsEmpty() ? ColumnKey : Property->GetName());
			FString Value;
			if (ObjectOrNull)
			{
				const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ObjectOrNull);
				Value = PropertyToString(Property, reinterpret_cast<const uint8*>(ValuePtr));
			}
			AddColumn(ColumnName, Value);
		}

		if (FExportableMetaData::HasCsvExpand(Property))
		{
			if (CastField<FSetProperty>(Property))
			{
				UE_LOG(LogTemp, Warning, TEXT("CsvExpand is not supported on TSet: %s"), *Property->GetName());
				continue;
			}

			const FString ExpandPrefix = Property->GetName() + TEXT("_");

			if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				if (!ObjectOrNull)
					continue;
				const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(ObjectOrNull);
				FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
				if (FStructProperty* StructInner = CastField<FStructProperty>(ArrayProp->Inner))
				{
					for (int32 Index = 0; Index < Helper.Num(); ++Index)
					{
						const void* ElemPtr = Helper.GetRawPtr(Index);
						const FString ElemPrefix = Prefix + ExpandPrefix + FString::FromInt(Index) + TEXT("_");
						ExportStructToColumns(ElemPtr, StructInner->Struct, InOutColumnToValue, InOutColumnOrder, ElemPrefix, Visited);
					}
				}
				else if (GetObjectPropertyClass(ArrayProp->Inner) != nullptr)
				{
					for (int32 Index = 0; Index < Helper.Num(); ++Index)
					{
						const void* ElemPtr = Helper.GetRawPtr(Index);
						UObject* ElemObj = ResolveObjectPropertyValueFromContainerPtr(ElemPtr, ArrayProp->Inner, true);
						if (!ElemObj)
							continue;
						if (!CanExportClass(ElemObj->GetClass()))
							continue;
						const FString ElemPrefix = Prefix + ExpandPrefix + FString::FromInt(Index) + TEXT("_");
						ExportObjectToColumns(ElemObj, ElemObj->GetClass(), InOutColumnToValue, InOutColumnOrder, ElemPrefix, Visited);
					}
				}
				else
				{
					for (int32 Index = 0; Index < Helper.Num(); ++Index)
					{
						const void* ElemPtr = Helper.GetRawPtr(Index);
						const FString ColumnName = Prefix + ExpandPrefix + FString::FromInt(Index);
						const FString Value = PropertyToString(ArrayProp->Inner, reinterpret_cast<const uint8*>(ElemPtr));
						AddColumn(ColumnName, Value);
					}
				}
				continue;
			}

			if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
			{
				if (!ObjectOrNull)
					continue;
				const void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(ObjectOrNull);
				FScriptMapHelper Helper(MapProp, MapPtr);
				if (FStructProperty* StructValue = CastField<FStructProperty>(MapProp->ValueProp))
				{
					for (FScriptMapHelper::FIterator MapIt = Helper.CreateIterator(); MapIt; ++MapIt)
					{
						const uint8* KeyPtr = Helper.GetKeyPtr(MapIt);
						const uint8* ValuePtr = Helper.GetValuePtr(MapIt);
						const FString KeyString = PropertyToString(MapProp->KeyProp, KeyPtr);
						const FString ElemPrefix = Prefix + ExpandPrefix + KeyString + TEXT("_");
						ExportStructToColumns(ValuePtr, StructValue->Struct, InOutColumnToValue, InOutColumnOrder, ElemPrefix, Visited);
					}
				}
				else if (GetObjectPropertyClass(MapProp->ValueProp) != nullptr)
				{
					for (FScriptMapHelper::FIterator MapIt = Helper.CreateIterator(); MapIt; ++MapIt)
					{
						const uint8* KeyPtr = Helper.GetKeyPtr(MapIt);
						const uint8* ValuePtr = Helper.GetValuePtr(MapIt);
						const FString KeyString = PropertyToString(MapProp->KeyProp, KeyPtr);
						UObject* ValObj = ResolveObjectPropertyValueFromContainerPtr(ValuePtr, MapProp->ValueProp, true);
						if (!ValObj)
							continue;
						if (!CanExportClass(ValObj->GetClass()))
							continue;
						const FString ElemPrefix = Prefix + ExpandPrefix + KeyString + TEXT("_");
						ExportObjectToColumns(ValObj, ValObj->GetClass(), InOutColumnToValue, InOutColumnOrder, ElemPrefix, Visited);
					}
				}
				else
				{
					for (FScriptMapHelper::FIterator MapIt = Helper.CreateIterator(); MapIt; ++MapIt)
					{
						const uint8* KeyPtr = Helper.GetKeyPtr(MapIt);
						const uint8* ValuePtr = Helper.GetValuePtr(MapIt);
						const FString KeyString = PropertyToString(MapProp->KeyProp, KeyPtr);
						const FString ColumnName = Prefix + ExpandPrefix + KeyString;
						const FString Value = PropertyToString(MapProp->ValueProp, ValuePtr);
						AddColumn(ColumnName, Value);
					}
				}
				continue;
			}

			UClass* InnerClass = GetObjectPropertyClass(Property);
			if (!InnerClass || !CanExportClass(InnerClass))
				continue;

			UObject* InnerObject = ObjectOrNull ? ResolveObjectPropertyValue(ObjectOrNull, Property, true) : nullptr;
			if (InnerObject)
			{
				ExportObjectToColumns(InnerObject, InnerObject->GetClass(), InOutColumnToValue, InOutColumnOrder, Prefix + ExpandPrefix, Visited);
			}
			else
			{
				ExportClassColumnsEmpty(InnerClass, InOutColumnToValue, InOutColumnOrder, Prefix + ExpandPrefix);
			}
		}
	}

	if (UClass* SuperClass = Class->GetSuperClass())
	{
		ExportObjectToColumns(ObjectOrNull, SuperClass, InOutColumnToValue, InOutColumnOrder, Prefix, Visited);
	}
}

UClass* UAssetCsvSyncCSVHandler::GetObjectPropertyClass(FProperty* Property)
{
	if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(Property))
	{
		return SoftProp->PropertyClass;
	}
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		return ObjProp->PropertyClass;
	}
	return nullptr;
}

UObject* UAssetCsvSyncCSVHandler::ResolveObjectPropertyValue(UObject* Container, FProperty* Property, bool bLoadSoft)
{
	if (!Container || !Property)
		return nullptr;

	if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(Property))
	{
		const void* ValuePtr = SoftProp->ContainerPtrToValuePtr<void>(Container);
		const FSoftObjectPtr SoftPtr = SoftProp->GetPropertyValue(ValuePtr);
		return bLoadSoft ? SoftPtr.LoadSynchronous() : SoftPtr.Get();
	}
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		const void* ValuePtr = ObjProp->ContainerPtrToValuePtr<void>(Container);
		return ObjProp->GetPropertyValue(ValuePtr);
	}
	return nullptr;
}

UObject* UAssetCsvSyncCSVHandler::ResolveObjectPropertyValueFromContainerPtr(const void* ContainerPtr, FProperty* Property, bool bLoadSoft)
{
	if (!ContainerPtr || !Property)
		return nullptr;

	if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(Property))
	{
		const void* ValuePtr = SoftProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		const FSoftObjectPtr SoftPtr = SoftProp->GetPropertyValue(ValuePtr);
		return bLoadSoft ? SoftPtr.LoadSynchronous() : SoftPtr.Get();
	}
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		const void* ValuePtr = ObjProp->ContainerPtrToValuePtr<void>(ContainerPtr);
		return ObjProp->GetPropertyValue(ValuePtr);
	}
	return nullptr;
}

void UAssetCsvSyncCSVHandler::ExportStructToColumns(const void* StructPtr, UScriptStruct* Struct, TMap<FString, FString>& InOutColumnToValue, TArray<FString>& InOutColumnOrder, const FString& Prefix, TSet<const UObject*>& Visited)
{
	if (!Struct || !StructPtr)
		return;

	auto AddColumn = [&InOutColumnToValue, &InOutColumnOrder](const FString& Name, const FString& Value)
	{
		if (!InOutColumnToValue.Contains(Name))
		{
			InOutColumnOrder.Add(Name);
		}
		InOutColumnToValue.Add(Name, Value);
	};

	for (TFieldIterator<FProperty> It(Struct, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
			continue;

		// Struct expansion exports all leaf fields by default.
		// If CsvColumn is present, it overrides the column name.
		// If CsvExpand is present without CsvColumn, we do not export a leaf column for the property.
		if (FExportableMetaData::HasCsvColumn(Property) || !FExportableMetaData::HasCsvExpand(Property))
		{
			const FString ColumnKey = FExportableMetaData::HasCsvColumn(Property) ? FExportableMetaData::GetCsvColumn(Property) : Property->GetName();
			const FString ColumnName = Prefix + (!ColumnKey.IsEmpty() ? ColumnKey : Property->GetName());
			const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructPtr);
			AddColumn(ColumnName, PropertyToString(Property, reinterpret_cast<const uint8*>(ValuePtr)));
		}

		if (!FExportableMetaData::HasCsvExpand(Property))
			continue;

		if (CastField<FSetProperty>(Property))
		{
			UE_LOG(LogTemp, Warning, TEXT("CsvExpand is not supported on TSet: %s"), *Property->GetName());
			continue;
		}

		const FString ExpandPrefix = Property->GetName() + TEXT("_");

		if (FStructProperty* NestedStruct = CastField<FStructProperty>(Property))
		{
			const void* NestedPtr = NestedStruct->ContainerPtrToValuePtr<void>(StructPtr);
			ExportStructToColumns(NestedPtr, NestedStruct->Struct, InOutColumnToValue, InOutColumnOrder, Prefix + ExpandPrefix, Visited);
			continue;
		}

		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(StructPtr);
			FScriptArrayHelper Helper(ArrayProp, ArrayPtr);

			if (FStructProperty* StructInner = CastField<FStructProperty>(ArrayProp->Inner))
			{
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					const void* ElemPtr = Helper.GetRawPtr(Index);
					const FString ElemPrefix = Prefix + ExpandPrefix + FString::FromInt(Index) + TEXT("_");
					ExportStructToColumns(ElemPtr, StructInner->Struct, InOutColumnToValue, InOutColumnOrder, ElemPrefix, Visited);
				}
			}
			else if (GetObjectPropertyClass(ArrayProp->Inner) != nullptr)
			{
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					const void* ElemPtr = Helper.GetRawPtr(Index);
					UObject* ElemObj = ResolveObjectPropertyValueFromContainerPtr(ElemPtr, ArrayProp->Inner, true);
					if (!ElemObj)
						continue;
					if (!CanExportClass(ElemObj->GetClass()))
						continue;
					const FString ElemPrefix = Prefix + ExpandPrefix + FString::FromInt(Index) + TEXT("_");
					ExportObjectToColumns(ElemObj, ElemObj->GetClass(), InOutColumnToValue, InOutColumnOrder, ElemPrefix, Visited);
				}
			}
			else
			{
				for (int32 Index = 0; Index < Helper.Num(); ++Index)
				{
					const void* ElemPtr = Helper.GetRawPtr(Index);
					const FString ColumnName = Prefix + ExpandPrefix + FString::FromInt(Index);
					const FString Value = PropertyToString(ArrayProp->Inner, reinterpret_cast<const uint8*>(ElemPtr));
					AddColumn(ColumnName, Value);
				}
			}
			continue;
		}

		if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
		{
			const void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(StructPtr);
			FScriptMapHelper Helper(MapProp, MapPtr);

			if (FStructProperty* StructValue = CastField<FStructProperty>(MapProp->ValueProp))
			{
				for (FScriptMapHelper::FIterator MapIt = Helper.CreateIterator(); MapIt; ++MapIt)
				{
					const uint8* KeyPtr = Helper.GetKeyPtr(MapIt);
					const uint8* ValuePtr = Helper.GetValuePtr(MapIt);
					const FString KeyString = PropertyToString(MapProp->KeyProp, KeyPtr);
					const FString ElemPrefix = Prefix + ExpandPrefix + KeyString + TEXT("_");
					ExportStructToColumns(ValuePtr, StructValue->Struct, InOutColumnToValue, InOutColumnOrder, ElemPrefix, Visited);
				}
			}
			else if (GetObjectPropertyClass(MapProp->ValueProp) != nullptr)
			{
				for (FScriptMapHelper::FIterator MapIt = Helper.CreateIterator(); MapIt; ++MapIt)
				{
					const uint8* KeyPtr = Helper.GetKeyPtr(MapIt);
					const uint8* ValuePtr = Helper.GetValuePtr(MapIt);
					const FString KeyString = PropertyToString(MapProp->KeyProp, KeyPtr);
					UObject* ValObj = ResolveObjectPropertyValueFromContainerPtr(ValuePtr, MapProp->ValueProp, true);
					if (!ValObj)
						continue;
					if (!CanExportClass(ValObj->GetClass()))
						continue;
					const FString ElemPrefix = Prefix + ExpandPrefix + KeyString + TEXT("_");
					ExportObjectToColumns(ValObj, ValObj->GetClass(), InOutColumnToValue, InOutColumnOrder, ElemPrefix, Visited);
				}
			}
			else
			{
				for (FScriptMapHelper::FIterator MapIt = Helper.CreateIterator(); MapIt; ++MapIt)
				{
					const uint8* KeyPtr = Helper.GetKeyPtr(MapIt);
					const uint8* ValuePtr = Helper.GetValuePtr(MapIt);
					const FString KeyString = PropertyToString(MapProp->KeyProp, KeyPtr);
					const FString ColumnName = Prefix + ExpandPrefix + KeyString;
					const FString Value = PropertyToString(MapProp->ValueProp, ValuePtr);
					AddColumn(ColumnName, Value);
				}
			}
			continue;
		}

		UObject* InnerObject = ResolveObjectPropertyValueFromContainerPtr(StructPtr, Property, true);
		if (!InnerObject)
			continue;
		if (!CanExportClass(InnerObject->GetClass()))
			continue;
		ExportObjectToColumns(InnerObject, InnerObject->GetClass(), InOutColumnToValue, InOutColumnOrder, Prefix + ExpandPrefix, Visited);
	}

	if (UScriptStruct* SuperStruct = Cast<UScriptStruct>(Struct->GetSuperStruct()))
	{
		ExportStructToColumns(StructPtr, SuperStruct, InOutColumnToValue, InOutColumnOrder, Prefix, Visited);
	}
}

bool UAssetCsvSyncCSVHandler::ApplyColumnsToStruct(void* StructPtr, UScriptStruct* Struct, const TMap<FString, FString>& ColumnToValue, const FString& Prefix, TSet<const UObject*>& Visited)
{
	if (!Struct || !StructPtr)
		return false;

	const EAssetCsvSyncWriteBackScope Scope = UAssetCsvSyncEditorPluginSettings::Get()->WriteBackScope;

	for (TFieldIterator<FProperty> It(Struct, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
			continue;

		// Struct expansion imports all leaf fields by default.
		if (FExportableMetaData::HasCsvColumn(Property) || !FExportableMetaData::HasCsvExpand(Property))
		{
			const FString ColumnKey = FExportableMetaData::HasCsvColumn(Property) ? FExportableMetaData::GetCsvColumn(Property) : Property->GetName();
			const FString ColumnName = Prefix + (!ColumnKey.IsEmpty() ? ColumnKey : Property->GetName());
			if (const FString* Found = ColumnToValue.Find(ColumnName))
			{
				void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructPtr);
				StringToProperty(Property, reinterpret_cast<uint8*>(ValuePtr), *Found);
			}
		}

		if (Scope != EAssetCsvSyncWriteBackScope::RootAndExpanded || !FExportableMetaData::HasCsvExpand(Property))
			continue;

		if (CastField<FSetProperty>(Property))
		{
			UE_LOG(LogTemp, Warning, TEXT("CsvExpand is not supported on TSet: %s"), *Property->GetName());
			continue;
		}

		const FString ExpandPrefix = Property->GetName() + TEXT("_");
		const FString FullExpandPrefix = Prefix + ExpandPrefix;

		if (FStructProperty* NestedStruct = CastField<FStructProperty>(Property))
		{
			void* NestedPtr = NestedStruct->ContainerPtrToValuePtr<void>(StructPtr);
			ApplyColumnsToStruct(NestedPtr, NestedStruct->Struct, ColumnToValue, FullExpandPrefix, Visited);
			continue;
		}

		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(StructPtr);
			FScriptArrayHelper Helper(ArrayProp, ArrayPtr);

			if (FStructProperty* StructInner = CastField<FStructProperty>(ArrayProp->Inner))
			{
				TSet<int32> Indices;
				int32 MaxIndex = -1;
				for (const TPair<FString, FString>& Pair : ColumnToValue)
				{
					if (!Pair.Key.StartsWith(FullExpandPrefix))
						continue;
					const FString Suffix = Pair.Key.Mid(FullExpandPrefix.Len());
					int32 UnderscorePos = INDEX_NONE;
					if (!Suffix.FindChar(TEXT('_'), UnderscorePos) || UnderscorePos <= 0)
						continue;
					const FString IndexStr = Suffix.Left(UnderscorePos);
					if (!IndexStr.IsNumeric())
						continue;
					const int32 Index = FCString::Atoi(*IndexStr);
					if (Index < 0)
						continue;
					Indices.Add(Index);
					MaxIndex = FMath::Max(MaxIndex, Index);
				}
				if (MaxIndex >= 0 && MaxIndex >= Helper.Num())
				{
					Helper.Resize(MaxIndex + 1);
				}
				for (int32 Index : Indices)
				{
					if (Index < 0 || Index >= Helper.Num())
						continue;
					void* ElemPtr = Helper.GetRawPtr(Index);
					const FString ElemPrefix = FullExpandPrefix + FString::FromInt(Index) + TEXT("_");
					ApplyColumnsToStruct(ElemPtr, StructInner->Struct, ColumnToValue, ElemPrefix, Visited);
				}
				continue;
			}

			if (GetObjectPropertyClass(ArrayProp->Inner) != nullptr)
			{
				TSet<int32> Indices;
				int32 MaxIndex = -1;
				for (const TPair<FString, FString>& Pair : ColumnToValue)
				{
					if (!Pair.Key.StartsWith(FullExpandPrefix))
						continue;
					const FString Suffix = Pair.Key.Mid(FullExpandPrefix.Len());
					int32 UnderscorePos = INDEX_NONE;
					if (!Suffix.FindChar(TEXT('_'), UnderscorePos) || UnderscorePos <= 0)
						continue;
					const FString IndexStr = Suffix.Left(UnderscorePos);
					if (!IndexStr.IsNumeric())
						continue;
					const int32 Index = FCString::Atoi(*IndexStr);
					if (Index < 0)
						continue;
					Indices.Add(Index);
					MaxIndex = FMath::Max(MaxIndex, Index);
				}
				if (MaxIndex >= 0 && MaxIndex >= Helper.Num())
				{
					Helper.Resize(MaxIndex + 1);
				}
				for (int32 Index : Indices)
				{
					if (Index < 0 || Index >= Helper.Num())
						continue;
					void* ElemPtr = Helper.GetRawPtr(Index);
					UObject* ElemObj = ResolveObjectPropertyValueFromContainerPtr(ElemPtr, ArrayProp->Inner, true);
					if (!ElemObj)
						continue;
					if (!CanExportClass(ElemObj->GetClass()))
						continue;
					const FString ElemPrefix = FullExpandPrefix + FString::FromInt(Index) + TEXT("_");
					ApplyColumnsToObject(ElemObj, ElemObj->GetClass(), ColumnToValue, ElemPrefix, Visited);
				}
				continue;
			}

			// Primitive elements: ${Prefix}${Index}
			TArray<TPair<int32, const FString*>> Writes;
			for (const TPair<FString, FString>& Pair : ColumnToValue)
			{
				if (!Pair.Key.StartsWith(FullExpandPrefix))
					continue;
				const FString Suffix = Pair.Key.Mid(FullExpandPrefix.Len());
				if (Suffix.IsEmpty() || !Suffix.IsNumeric())
					continue;
				const int32 Index = FCString::Atoi(*Suffix);
				if (Index < 0)
					continue;
				Writes.Add(TPair<int32, const FString*>(Index, &Pair.Value));
			}
			Writes.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });
			for (const auto& W : Writes)
			{
				if (W.Key >= Helper.Num())
				{
					Helper.Resize(W.Key + 1);
				}
				void* ElemPtr = Helper.GetRawPtr(W.Key);
				StringToProperty(ArrayProp->Inner, reinterpret_cast<uint8*>(ElemPtr), *W.Value);
			}
			continue;
		}

		if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
		{
			void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(StructPtr);
			FScriptMapHelper Helper(MapProp, MapPtr);
			bool bNeedsRehash = false;

			if (FStructProperty* StructValue = CastField<FStructProperty>(MapProp->ValueProp))
			{
				TSet<FString> Keys;
				for (const TPair<FString, FString>& Pair : ColumnToValue)
				{
					if (!Pair.Key.StartsWith(FullExpandPrefix))
						continue;
					const FString Suffix = Pair.Key.Mid(FullExpandPrefix.Len());
					int32 UnderscorePos = INDEX_NONE;
					if (!Suffix.FindChar(TEXT('_'), UnderscorePos) || UnderscorePos <= 0)
						continue;
					Keys.Add(Suffix.Left(UnderscorePos));
				}

				TArray<uint8> TempKeyStorage;
				TempKeyStorage.SetNumZeroed(MapProp->KeyProp->GetSize());
				MapProp->KeyProp->InitializeValue(TempKeyStorage.GetData());

				for (const FString& KeyString : Keys)
				{
					MapProp->KeyProp->ClearValue(TempKeyStorage.GetData());
					if (!StringToProperty(MapProp->KeyProp, TempKeyStorage.GetData(), KeyString))
						continue;
					int32 FoundIndex = Helper.FindMapIndexWithKey(TempKeyStorage.GetData());
					if (FoundIndex == INDEX_NONE)
					{
						const int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
						MapProp->KeyProp->CopyCompleteValue(Helper.GetKeyPtr(NewIndex), TempKeyStorage.GetData());
						bNeedsRehash = true;
						FoundIndex = NewIndex;
					}
					void* ValuePtr = Helper.GetValuePtr(FoundIndex);
					const FString ElemPrefix = FullExpandPrefix + KeyString + TEXT("_");
					ApplyColumnsToStruct(ValuePtr, StructValue->Struct, ColumnToValue, ElemPrefix, Visited);
				}
				MapProp->KeyProp->DestroyValue(TempKeyStorage.GetData());
				if (bNeedsRehash)
				{
					Helper.Rehash();
				}
				continue;
			}

			if (GetObjectPropertyClass(MapProp->ValueProp) != nullptr)
			{
				TSet<FString> Keys;
				for (const TPair<FString, FString>& Pair : ColumnToValue)
				{
					if (!Pair.Key.StartsWith(FullExpandPrefix))
						continue;
					const FString Suffix = Pair.Key.Mid(FullExpandPrefix.Len());
					int32 UnderscorePos = INDEX_NONE;
					if (!Suffix.FindChar(TEXT('_'), UnderscorePos) || UnderscorePos <= 0)
						continue;
					Keys.Add(Suffix.Left(UnderscorePos));
				}

				TArray<uint8> TempKeyStorage;
				TempKeyStorage.SetNumZeroed(MapProp->KeyProp->GetSize());
				MapProp->KeyProp->InitializeValue(TempKeyStorage.GetData());

				for (const FString& KeyString : Keys)
				{
					MapProp->KeyProp->ClearValue(TempKeyStorage.GetData());
					if (!StringToProperty(MapProp->KeyProp, TempKeyStorage.GetData(), KeyString))
						continue;
					int32 FoundIndex = Helper.FindMapIndexWithKey(TempKeyStorage.GetData());
					if (FoundIndex == INDEX_NONE)
					{
						const int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
						MapProp->KeyProp->CopyCompleteValue(Helper.GetKeyPtr(NewIndex), TempKeyStorage.GetData());
						bNeedsRehash = true;
						FoundIndex = NewIndex;
					}
					void* ValuePtr = Helper.GetValuePtr(FoundIndex);
					UObject* ValObj = ResolveObjectPropertyValueFromContainerPtr(ValuePtr, MapProp->ValueProp, true);
					if (!ValObj)
						continue;
					if (!CanExportClass(ValObj->GetClass()))
						continue;
					const FString ElemPrefix = FullExpandPrefix + KeyString + TEXT("_");
					ApplyColumnsToObject(ValObj, ValObj->GetClass(), ColumnToValue, ElemPrefix, Visited);
				}

				MapProp->KeyProp->DestroyValue(TempKeyStorage.GetData());
				if (bNeedsRehash)
				{
					Helper.Rehash();
				}
				continue;
			}

			// Primitive values: ${Prefix}${Key}
			TArray<uint8> TempKeyStorage;
			TempKeyStorage.SetNumZeroed(MapProp->KeyProp->GetSize());
			MapProp->KeyProp->InitializeValue(TempKeyStorage.GetData());

			for (const TPair<FString, FString>& Pair : ColumnToValue)
			{
				if (!Pair.Key.StartsWith(FullExpandPrefix))
					continue;
				const FString KeyString = Pair.Key.Mid(FullExpandPrefix.Len());
				if (KeyString.IsEmpty())
					continue;

				MapProp->KeyProp->ClearValue(TempKeyStorage.GetData());
				if (!StringToProperty(MapProp->KeyProp, TempKeyStorage.GetData(), KeyString))
					continue;

				int32 FoundIndex = Helper.FindMapIndexWithKey(TempKeyStorage.GetData());
				if (FoundIndex == INDEX_NONE)
				{
					const int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
					MapProp->KeyProp->CopyCompleteValue(Helper.GetKeyPtr(NewIndex), TempKeyStorage.GetData());
					StringToProperty(MapProp->ValueProp, Helper.GetValuePtr(NewIndex), Pair.Value);
					bNeedsRehash = true;
				}
				else
				{
					StringToProperty(MapProp->ValueProp, Helper.GetValuePtr(FoundIndex), Pair.Value);
				}
			}

			if (bNeedsRehash)
			{
				Helper.Rehash();
			}
			MapProp->KeyProp->DestroyValue(TempKeyStorage.GetData());
			continue;
		}

		UObject* InnerObject = ResolveObjectPropertyValueFromContainerPtr(StructPtr, Property, true);
		if (!InnerObject)
			continue;
		if (!CanExportClass(InnerObject->GetClass()))
			continue;
		ApplyColumnsToObject(InnerObject, InnerObject->GetClass(), ColumnToValue, FullExpandPrefix, Visited);
	}

	if (UScriptStruct* SuperStruct = Cast<UScriptStruct>(Struct->GetSuperStruct()))
	{
		ApplyColumnsToStruct(StructPtr, SuperStruct, ColumnToValue, Prefix, Visited);
	}

	return true;
}

bool UAssetCsvSyncCSVHandler::ApplyCSVRowToObject(UObject* TargetObject, UClass* TargetClass, const TArray<FString>& Headers, const TArray<FString>& Values)
{
	if (!TargetObject || !TargetClass)
		return false;
	if (!CanExportClass(TargetClass))
		return false;
	if (Headers.Num() != Values.Num())
		return false;

	TMap<FString, FString> ColumnToValue;
	ColumnToValue.Reserve(Headers.Num());
	for (int32 i = 0; i < Headers.Num(); ++i)
	{
		ColumnToValue.Add(Headers[i], Values[i]);
	}

	TSet<const UObject*> Visited;
	return ApplyColumnsToObject(TargetObject, TargetClass, ColumnToValue, FString(), Visited);
}

bool UAssetCsvSyncCSVHandler::ApplyColumnsToObject(UObject* TargetObject, UClass* TargetClass, const TMap<FString, FString>& ColumnToValue, const FString& Prefix, TSet<const UObject*>& Visited)
{
	if (!TargetObject || !TargetClass)
		return false;
	if (Visited.Contains(TargetObject))
		return true;
	Visited.Add(TargetObject);

	const EAssetCsvSyncWriteBackScope Scope = UAssetCsvSyncEditorPluginSettings::Get()->WriteBackScope;

	for (TFieldIterator<FProperty> It(TargetClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
			continue;

		// 1) Leaf (CsvColumn) always imports
		if (FExportableMetaData::HasCsvColumn(Property))
		{
			const FString ColumnKey = FExportableMetaData::GetCsvColumn(Property);
			const FString ColumnName = Prefix + (!ColumnKey.IsEmpty() ? ColumnKey : Property->GetName());
			if (const FString* Found = ColumnToValue.Find(ColumnName))
			{
				void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
				StringToProperty(Property, reinterpret_cast<uint8*>(ValuePtr), *Found);
			}
		}

		// 2) Expand (CsvExpand) is gated by WriteBackScope
		if (Scope != EAssetCsvSyncWriteBackScope::RootAndExpanded || !FExportableMetaData::HasCsvExpand(Property))
			continue;

		if (CastField<FSetProperty>(Property))
		{
			UE_LOG(LogTemp, Warning, TEXT("CsvExpand is not supported on TSet: %s"), *Property->GetName());
			continue;
		}

		const FString ExpandPrefix = Property->GetName() + TEXT("_");
		const FString FullExpandPrefix = Prefix + ExpandPrefix;

		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(TargetObject);
			FScriptArrayHelper Helper(ArrayProp, ArrayPtr);

			if (FStructProperty* StructInner = CastField<FStructProperty>(ArrayProp->Inner))
			{
				TSet<int32> Indices;
				int32 MaxIndex = -1;
				for (const TPair<FString, FString>& Pair : ColumnToValue)
				{
					if (!Pair.Key.StartsWith(FullExpandPrefix))
						continue;
					const FString Suffix = Pair.Key.Mid(FullExpandPrefix.Len());
					int32 UnderscorePos = INDEX_NONE;
					if (!Suffix.FindChar(TEXT('_'), UnderscorePos) || UnderscorePos <= 0)
						continue;
					const FString IndexStr = Suffix.Left(UnderscorePos);
					if (!IndexStr.IsNumeric())
						continue;
					const int32 Index = FCString::Atoi(*IndexStr);
					if (Index < 0)
						continue;
					Indices.Add(Index);
					MaxIndex = FMath::Max(MaxIndex, Index);
				}

				if (MaxIndex >= 0 && MaxIndex >= Helper.Num())
				{
					Helper.Resize(MaxIndex + 1);
				}

				for (int32 Index : Indices)
				{
					if (Index < 0 || Index >= Helper.Num())
						continue;
					void* ElemPtr = Helper.GetRawPtr(Index);
					const FString ElemPrefix = FullExpandPrefix + FString::FromInt(Index) + TEXT("_");
					ApplyColumnsToStruct(ElemPtr, StructInner->Struct, ColumnToValue, ElemPrefix, Visited);
				}
				continue;
			}

			if (GetObjectPropertyClass(ArrayProp->Inner) != nullptr)
			{
				TSet<int32> Indices;
				int32 MaxIndex = -1;
				for (const TPair<FString, FString>& Pair : ColumnToValue)
				{
					if (!Pair.Key.StartsWith(FullExpandPrefix))
						continue;
					const FString Suffix = Pair.Key.Mid(FullExpandPrefix.Len());
					int32 UnderscorePos = INDEX_NONE;
					if (!Suffix.FindChar(TEXT('_'), UnderscorePos) || UnderscorePos <= 0)
						continue;
					const FString IndexStr = Suffix.Left(UnderscorePos);
					if (!IndexStr.IsNumeric())
						continue;
					const int32 Index = FCString::Atoi(*IndexStr);
					if (Index < 0)
						continue;
					Indices.Add(Index);
					MaxIndex = FMath::Max(MaxIndex, Index);
				}

				if (MaxIndex >= 0 && MaxIndex >= Helper.Num())
				{
					Helper.Resize(MaxIndex + 1);
				}

				for (int32 Index : Indices)
				{
					if (Index < 0 || Index >= Helper.Num())
						continue;
					void* ElemPtr = Helper.GetRawPtr(Index);
					UObject* ElemObj = ResolveObjectPropertyValueFromContainerPtr(ElemPtr, ArrayProp->Inner, true);
					if (!ElemObj)
						continue;
					if (!CanExportClass(ElemObj->GetClass()))
						continue;
					const FString ElemPrefix = FullExpandPrefix + FString::FromInt(Index) + TEXT("_");
					ApplyColumnsToObject(ElemObj, ElemObj->GetClass(), ColumnToValue, ElemPrefix, Visited);
				}
				continue;
			}

			TArray<TPair<int32, const FString*>> Writes;
			for (const TPair<FString, FString>& Pair : ColumnToValue)
			{
				if (!Pair.Key.StartsWith(FullExpandPrefix))
					continue;
				const FString Suffix = Pair.Key.Mid(FullExpandPrefix.Len());
				if (Suffix.IsEmpty() || !Suffix.IsNumeric())
					continue;
				const int32 Index = FCString::Atoi(*Suffix);
				if (Index < 0)
					continue;
				Writes.Add(TPair<int32, const FString*>(Index, &Pair.Value));
			}
			Writes.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });
			for (const auto& W : Writes)
			{
				if (W.Key >= Helper.Num())
				{
					Helper.Resize(W.Key + 1);
				}
				void* ElemPtr = Helper.GetRawPtr(W.Key);
				StringToProperty(ArrayProp->Inner, reinterpret_cast<uint8*>(ElemPtr), *W.Value);
			}
			continue;
		}

		if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
		{
			void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(TargetObject);
			FScriptMapHelper Helper(MapProp, MapPtr);
			bool bNeedsRehash = false;

			if (FStructProperty* StructValue = CastField<FStructProperty>(MapProp->ValueProp))
			{
				TSet<FString> Keys;
				for (const TPair<FString, FString>& Pair : ColumnToValue)
				{
					if (!Pair.Key.StartsWith(FullExpandPrefix))
						continue;
					const FString Suffix = Pair.Key.Mid(FullExpandPrefix.Len());
					int32 UnderscorePos = INDEX_NONE;
					if (!Suffix.FindChar(TEXT('_'), UnderscorePos) || UnderscorePos <= 0)
						continue;
					Keys.Add(Suffix.Left(UnderscorePos));
				}

				TArray<uint8> TempKeyStorage;
				TempKeyStorage.SetNumZeroed(MapProp->KeyProp->GetSize());
				MapProp->KeyProp->InitializeValue(TempKeyStorage.GetData());

				for (const FString& KeyString : Keys)
				{
					MapProp->KeyProp->ClearValue(TempKeyStorage.GetData());
					if (!StringToProperty(MapProp->KeyProp, TempKeyStorage.GetData(), KeyString))
						continue;

					int32 FoundIndex = Helper.FindMapIndexWithKey(TempKeyStorage.GetData());
					if (FoundIndex == INDEX_NONE)
					{
						const int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
						MapProp->KeyProp->CopyCompleteValue(Helper.GetKeyPtr(NewIndex), TempKeyStorage.GetData());
						bNeedsRehash = true;
						FoundIndex = NewIndex;
					}

					void* ValuePtr = Helper.GetValuePtr(FoundIndex);
					const FString ElemPrefix = FullExpandPrefix + KeyString + TEXT("_");
					ApplyColumnsToStruct(ValuePtr, StructValue->Struct, ColumnToValue, ElemPrefix, Visited);
				}

				MapProp->KeyProp->DestroyValue(TempKeyStorage.GetData());
				if (bNeedsRehash)
				{
					Helper.Rehash();
				}
				continue;
			}

			if (GetObjectPropertyClass(MapProp->ValueProp) != nullptr)
			{
				TSet<FString> Keys;
				for (const TPair<FString, FString>& Pair : ColumnToValue)
				{
					if (!Pair.Key.StartsWith(FullExpandPrefix))
						continue;
					const FString Suffix = Pair.Key.Mid(FullExpandPrefix.Len());
					int32 UnderscorePos = INDEX_NONE;
					if (!Suffix.FindChar(TEXT('_'), UnderscorePos) || UnderscorePos <= 0)
						continue;
					Keys.Add(Suffix.Left(UnderscorePos));
				}

				TArray<uint8> TempKeyStorage;
				TempKeyStorage.SetNumZeroed(MapProp->KeyProp->GetSize());
				MapProp->KeyProp->InitializeValue(TempKeyStorage.GetData());

				for (const FString& KeyString : Keys)
				{
					MapProp->KeyProp->ClearValue(TempKeyStorage.GetData());
					if (!StringToProperty(MapProp->KeyProp, TempKeyStorage.GetData(), KeyString))
						continue;

					int32 FoundIndex = Helper.FindMapIndexWithKey(TempKeyStorage.GetData());
					if (FoundIndex == INDEX_NONE)
					{
						const int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
						MapProp->KeyProp->CopyCompleteValue(Helper.GetKeyPtr(NewIndex), TempKeyStorage.GetData());
						bNeedsRehash = true;
						FoundIndex = NewIndex;
					}

					void* ValuePtr = Helper.GetValuePtr(FoundIndex);
					UObject* ValObj = ResolveObjectPropertyValueFromContainerPtr(ValuePtr, MapProp->ValueProp, true);
					if (!ValObj)
						continue;
					if (!CanExportClass(ValObj->GetClass()))
						continue;
					const FString ElemPrefix = FullExpandPrefix + KeyString + TEXT("_");
					ApplyColumnsToObject(ValObj, ValObj->GetClass(), ColumnToValue, ElemPrefix, Visited);
				}

				MapProp->KeyProp->DestroyValue(TempKeyStorage.GetData());
				if (bNeedsRehash)
				{
					Helper.Rehash();
				}
				continue;
			}

			TArray<uint8> TempKeyStorage;
			TempKeyStorage.SetNumZeroed(MapProp->KeyProp->GetSize());
			MapProp->KeyProp->InitializeValue(TempKeyStorage.GetData());

			for (const TPair<FString, FString>& Pair : ColumnToValue)
			{
				if (!Pair.Key.StartsWith(FullExpandPrefix))
					continue;
				const FString KeyString = Pair.Key.Mid(FullExpandPrefix.Len());
				if (KeyString.IsEmpty())
					continue;

				MapProp->KeyProp->ClearValue(TempKeyStorage.GetData());
				if (!StringToProperty(MapProp->KeyProp, TempKeyStorage.GetData(), KeyString))
					continue;

				int32 FoundIndex = Helper.FindMapIndexWithKey(TempKeyStorage.GetData());
				if (FoundIndex == INDEX_NONE)
				{
					const int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
					MapProp->KeyProp->CopyCompleteValue(Helper.GetKeyPtr(NewIndex), TempKeyStorage.GetData());
					StringToProperty(MapProp->ValueProp, Helper.GetValuePtr(NewIndex), Pair.Value);
					bNeedsRehash = true;
				}
				else
				{
					StringToProperty(MapProp->ValueProp, Helper.GetValuePtr(FoundIndex), Pair.Value);
				}
			}

			if (bNeedsRehash)
			{
				Helper.Rehash();
			}
			MapProp->KeyProp->DestroyValue(TempKeyStorage.GetData());
			continue;
		}

		UObject* InnerObject = ResolveObjectPropertyValue(TargetObject, Property, true);
		if (!InnerObject)
			continue;
		if (!CanExportClass(InnerObject->GetClass()))
			continue;
		ApplyColumnsToObject(InnerObject, InnerObject->GetClass(), ColumnToValue, FullExpandPrefix, Visited);
	}

	if (UClass* SuperClass = TargetClass->GetSuperClass())
	{
		ApplyColumnsToObject(TargetObject, SuperClass, ColumnToValue, Prefix, Visited);
	}

	return true;
}

FString UAssetCsvSyncCSVHandler::PropertyToString(FProperty* Property, const uint8* PropertyData)
{
	if (!Property || !PropertyData)
		return FString();

	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper Helper(ArrayProperty, PropertyData);
		TArray<FString> Items;
		Items.Reserve(Helper.Num());
		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			const uint8* ElemPtr = reinterpret_cast<const uint8*>(Helper.GetRawPtr(i));
			Items.Add(PropertyToString(ArrayProperty->Inner, ElemPtr));
		}
		return JoinListCell(Items);
	}
	else if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		FScriptSetHelper Helper(SetProperty, PropertyData);
		TArray<FString> Items;
		Items.Reserve(Helper.Num());
		for (FScriptSetHelper::FIterator It = Helper.CreateIterator(); It; ++It)
		{
			const uint8* ElemPtr = Helper.GetElementPtr(It);
			if (!ElemPtr)
				continue;
			Items.Add(PropertyToString(SetProperty->ElementProp, ElemPtr));
		}
		return JoinListCell(Items);
	}
	else if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper Helper(MapProperty, PropertyData);
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();

		for (FScriptMapHelper::FIterator It = Helper.CreateIterator(); It; ++It)
		{
			const uint8* KeyPtr = Helper.GetKeyPtr(It);
			const uint8* ValuePtr = Helper.GetValuePtr(It);
			if (!KeyPtr || !ValuePtr)
				continue;

			const FString KeyString = PropertyToString(MapProperty->KeyProp, KeyPtr);

			TSharedPtr<FJsonValue> JsonValue;
			if (FNumericProperty* NumProp = CastField<FNumericProperty>(MapProperty->ValueProp))
			{
				if (NumProp->IsInteger())
				{
					JsonValue = MakeShared<FJsonValueNumber>((double)NumProp->GetSignedIntPropertyValue(ValuePtr));
				}
				else
				{
					JsonValue = MakeShared<FJsonValueNumber>(NumProp->GetFloatingPointPropertyValue(ValuePtr));
				}
			}
			else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(MapProperty->ValueProp))
			{
				JsonValue = MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
			}
			else if (FStrProperty* StrProp = CastField<FStrProperty>(MapProperty->ValueProp))
			{
				JsonValue = MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValuePtr));
			}
			else if (FNameProperty* NameProp = CastField<FNameProperty>(MapProperty->ValueProp))
			{
				JsonValue = MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
			}
			else if (FTextProperty* TextProp = CastField<FTextProperty>(MapProperty->ValueProp))
			{
				JsonValue = MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
			}
			else
			{
				JsonValue = MakeShared<FJsonValueString>(PropertyToString(MapProperty->ValueProp, ValuePtr));
			}

			Obj->SetField(KeyString, JsonValue);
		}

		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
	{
		return FString::Printf(TEXT("%d"), IntProperty->GetPropertyValue(PropertyData));
	}
	else if (FInt64Property* Int64Property = CastField<FInt64Property>(Property))
	{
		return FString::Printf(TEXT("%lld"), Int64Property->GetPropertyValue(PropertyData));
	}
	else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
	{
		return FString::Printf(TEXT("%f"), FloatProperty->GetPropertyValue(PropertyData));
	}
	else if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
	{
		return FString::Printf(TEXT("%f"), DoubleProperty->GetPropertyValue(PropertyData));
	}
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		return BoolProperty->GetPropertyValue(PropertyData) ? TEXT("true") : TEXT("false");
	}
	else if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
	{
		return StringProperty->GetPropertyValue(PropertyData);
	}
	else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		return TextProperty->GetPropertyValue(PropertyData).ToString();
	}
	else if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
	{
		return NameProperty->GetPropertyValue(PropertyData).ToString();
	}
	else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
	{
		UObject* ObjectValue = ObjectProperty->GetPropertyValue(PropertyData);
		return ObjectValue ? ObjectValue->GetPathName() : TEXT("");
	}
	else if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
	{
		const FSoftObjectPtr SoftPtr = SoftObjectProperty->GetPropertyValue(PropertyData);
		return SoftPtr.ToSoftObjectPath().ToString();
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		return FString::Printf(TEXT("%d"), ByteProperty->GetPropertyValue(PropertyData));
	}

	// Fallback to UE text export for complex types
	FString Out;
	Property->ExportTextItem_Direct(Out, PropertyData, nullptr, nullptr, PPF_None);
	return Out;
}

bool UAssetCsvSyncCSVHandler::StringToProperty(FProperty* Property, uint8* PropertyData, const FString& StringValue)
{
	if (!Property || !PropertyData)
		return false;

	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper Helper(ArrayProperty, PropertyData);
		const TArray<FString> Items = ParseListCell(StringValue);
		Helper.Resize(Items.Num());
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			void* ElemPtr = Helper.GetRawPtr(i);
			StringToProperty(ArrayProperty->Inner, reinterpret_cast<uint8*>(ElemPtr), Items[i]);
		}
		return true;
	}
	else if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		FScriptSetHelper Helper(SetProperty, PropertyData);
		Helper.EmptyElements();
		const TArray<FString> Items = ParseListCell(StringValue);
		for (const FString& Item : Items)
		{
			const int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
			uint8* ElemPtr = Helper.GetElementPtr(NewIndex);
			if (!ElemPtr)
				continue;
			StringToProperty(SetProperty->ElementProp, ElemPtr, Item);
		}
		Helper.Rehash();
		return true;
	}
	else if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper Helper(MapProperty, PropertyData);
		Helper.EmptyValues();
		if (StringValue.IsEmpty())
		{
			return true;
		}

		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StringValue);
		if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("StringToProperty: Failed to parse JSON map cell"));
			return false;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Obj->Values)
		{
			const FString& KeyString = Pair.Key;
			const TSharedPtr<FJsonValue>& JsonVal = Pair.Value;
			if (!JsonVal.IsValid())
				continue;

			FString ValueString;
			switch (JsonVal->Type)
			{
			case EJson::String:
				ValueString = JsonVal->AsString();
				break;
			case EJson::Number:
				ValueString = FString::SanitizeFloat(JsonVal->AsNumber());
				break;
			case EJson::Boolean:
				ValueString = JsonVal->AsBool() ? TEXT("true") : TEXT("false");
				break;
			case EJson::Null:
				ValueString = FString();
				break;
			default:
				{
					FString Tmp;
					TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Tmp);
					FJsonSerializer::Serialize(JsonVal.ToSharedRef(), TEXT(""), Writer);
					ValueString = Tmp;
				}
				break;
			}

			const int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
			uint8* KeyPtr = Helper.GetKeyPtr(NewIndex);
			uint8* ValPtr = Helper.GetValuePtr(NewIndex);
			if (!KeyPtr || !ValPtr)
				continue;
			if (!StringToProperty(MapProperty->KeyProp, KeyPtr, KeyString))
				continue;
			StringToProperty(MapProperty->ValueProp, ValPtr, ValueString);
		}
		Helper.Rehash();
		return true;
	}

	if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
	{
		int32 Value = FCString::Atoi(*StringValue);
		IntProperty->SetPropertyValue(PropertyData, Value);
		return true;
	}
	else if (FInt64Property* Int64Property = CastField<FInt64Property>(Property))
	{
		int64 Value = FCString::Atoi64(*StringValue);
		Int64Property->SetPropertyValue(PropertyData, Value);
		return true;
	}
	else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
	{
		float Value = FCString::Atof(*StringValue);
		FloatProperty->SetPropertyValue(PropertyData, Value);
		return true;
	}
	else if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
	{
		double Value = FCString::Atod(*StringValue);
		DoubleProperty->SetPropertyValue(PropertyData, Value);
		return true;
	}
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		BoolProperty->SetPropertyValue(PropertyData, StringValue.ToBool());
		return true;
	}
	else if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
	{
		StringProperty->SetPropertyValue(PropertyData, StringValue);
		return true;
	}
	else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		TextProperty->SetPropertyValue(PropertyData, FText::FromString(StringValue));
		return true;
	}
	else if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
	{
		NameProperty->SetPropertyValue(PropertyData, FName(*StringValue));
		return true;
	}
	else if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
	{
		if (StringValue.IsEmpty())
		{
			SoftObjectProperty->SetPropertyValue(PropertyData, FSoftObjectPtr());
			return true;
		}
		const FSoftObjectPath Path(StringValue);
		SoftObjectProperty->SetPropertyValue(PropertyData, FSoftObjectPtr(Path));
		return true;
	}
	else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
	{
		if (StringValue.IsEmpty())
		{
			ObjectProperty->SetPropertyValue(PropertyData, nullptr);
			return true;
		}
		UObject* Loaded = StaticLoadObject(ObjectProperty->PropertyClass, nullptr, *StringValue);
		ObjectProperty->SetPropertyValue(PropertyData, Loaded);
		return true;
	}

	// Fallback to UE text import for complex types
	const TCHAR* Buffer = *StringValue;
	const TCHAR* Result = Property->ImportText_Direct(Buffer, PropertyData, nullptr, PPF_None);
	return Result != nullptr;
}

FString UAssetCsvSyncCSVHandler::EscapeCSVString(const FString& Value)
{
	if (Value.Contains(TEXT(",")) || Value.Contains(TEXT("\"")) || Value.Contains(TEXT("\n")) || Value.Contains(TEXT("\r")))
	{
		FString Escaped = Value.Replace(TEXT("\""), TEXT("\"\""));
		return TEXT("\"") + Escaped + TEXT("\"");
	}
	return Value;
}

TArray<FString> UAssetCsvSyncCSVHandler::ParseCSVLine(const FString& Line)
{
	TArray<FString> Result;
	FString CurrentValue;
	bool bInQuotes = false;

	for (int32 i = 0; i < Line.Len(); ++i)
	{
		TCHAR Char = Line[i];

		if (bInQuotes)
		{
			if (Char == TEXT('"'))
			{
				if (i + 1 < Line.Len() && Line[i + 1] == TEXT('"'))
				{
					CurrentValue += TEXT('"');
					++i;
				}
				else
				{
					bInQuotes = false;
				}
			}
			else
			{
				CurrentValue += Char;
			}
		}
		else
		{
			if (Char == TEXT('"'))
			{
				bInQuotes = true;
			}
			else if (Char == TEXT(','))
			{
				Result.Add(CurrentValue);
				CurrentValue.Reset();
			}
			else
			{
				CurrentValue += Char;
			}
		}
	}

	Result.Add(CurrentValue);
	return Result;
}

FString UAssetCsvSyncCSVHandler::EscapeListItem(const FString& Value)
{
	if (Value.Contains(TEXT(";")) || Value.Contains(TEXT("\"")))
	{
		FString Escaped = Value.Replace(TEXT("\""), TEXT("\"\""));
		return TEXT("\"") + Escaped + TEXT("\"");
	}
	return Value;
}

TArray<FString> UAssetCsvSyncCSVHandler::ParseListCell(const FString& Cell)
{
	TArray<FString> Result;
	if (Cell.IsEmpty())
	{
		return Result;
	}

	FString CurrentValue;
	bool bInQuotes = false;

	for (int32 i = 0; i < Cell.Len(); ++i)
	{
		const TCHAR Char = Cell[i];
		if (bInQuotes)
		{
			if (Char == TEXT('"'))
			{
				if (i + 1 < Cell.Len() && Cell[i + 1] == TEXT('"'))
				{
					CurrentValue += TEXT('"');
					++i;
				}
				else
				{
					bInQuotes = false;
				}
			}
			else
			{
				CurrentValue += Char;
			}
		}
		else
		{
			if (Char == TEXT('"'))
			{
				bInQuotes = true;
			}
			else if (Char == TEXT(';'))
			{
				Result.Add(CurrentValue);
				CurrentValue.Reset();
			}
			else
			{
				CurrentValue += Char;
			}
		}
	}

	Result.Add(CurrentValue);
	return Result;
}

FString UAssetCsvSyncCSVHandler::JoinListCell(const TArray<FString>& Items)
{
	FString Out;
	for (const FString& Item : Items)
	{
		Out += EscapeListItem(Item);
		Out += TEXT(";");
	}
	Out.RemoveFromEnd(TEXT(";"));
	return Out;
}

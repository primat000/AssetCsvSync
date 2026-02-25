// MIT Licensed. Copyright (c) 2026 Olga Taranova

#include "SheetTableCSVHandler.h"

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

#include "SheetTableEditorPluginSettings.h"

// TODO: check for nested structures, containers, wrong meta tags usage

bool USheetTableCSVHandler::ExportDataAssetToCSV(UDataAsset* DataAsset, const FString& FilePath)
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

	TArray<FExportColumnInfo> Columns;
	GetExportablePropertiesRecursively(Class, Columns);

	if (Columns.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ExportDataAssetToCSV: No exportable properties found for class %s"), *Class->GetName());
		return false;
	}

	FString CSVContent;

	for (const FExportColumnInfo& Col : Columns)
	{
		CSVContent += Col.ColumnName + TEXT(",");
	}
	CSVContent.RemoveFromEnd(TEXT(","));
	CSVContent += TEXT("\n");

	for (const FExportColumnInfo& Col : Columns)
	{
		FString Value;
		GetColumnValue(DataAsset, Col, Value);
		CSVContent += EscapeCSVString(Value) + TEXT(",");
	}
	CSVContent.RemoveFromEnd(TEXT(","));
	CSVContent += TEXT("\n");

	return FFileHelper::SaveStringToFile(CSVContent, *FilePath);
}

bool USheetTableCSVHandler::ImportCSVToDataAsset(const FString& FilePath, UDataAsset*& OutDataAsset, UClass* DataAssetClass)
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

bool USheetTableCSVHandler::ImportCSVToNewDataAssetAsset(const FString& FilePath, const FString& AssetPath, UClass* DataAssetClass, UDataAsset*& OutDataAsset, bool bSavePackage)
{
	OutDataAsset = nullptr;

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

bool USheetTableCSVHandler::CreateNewDataAssetAsset(const FString& AssetPath, UClass* DataAssetClass, UDataAsset*& OutDataAsset, bool bSavePackage)
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

TArray<FString> USheetTableCSVHandler::GetExportableDataAssetClasses()
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

bool USheetTableCSVHandler::SplitAssetPath(const FString& InAssetPath, FString& OutPackageName, FString& OutAssetName)
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

bool USheetTableCSVHandler::SaveCreatedAsset(UPackage* Package, UObject* AssetObject)
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

TArray<FString> USheetTableCSVHandler::GetExportableProperties(UClass* Class)
{
	TArray<FString> Result;

	TArray<FExportColumnInfo> Columns;
	GetExportablePropertiesRecursively(Class, Columns);

	for (const FExportColumnInfo& Col : Columns)
	{
		Result.Add(Col.ColumnName);
	}

	return Result;
}

bool USheetTableCSVHandler::CanExportClass(UClass* Class)
{
	if (!Class)
		return false;
	return FExportableMetaData::IsExportable(Class);
}

void USheetTableCSVHandler::GetExportablePropertiesRecursively(UClass* Class, TArray<FExportColumnInfo>& OutColumns)
{
	OutColumns.Reset();
	if (!Class)
		return;

	TArray<UClass*> ClassStack;
	BuildColumnsForClass(Class, OutColumns, ClassStack, TArray<FProperty*>(), FString());
}

void USheetTableCSVHandler::BuildColumnsForClass(UClass* Class, TArray<FExportColumnInfo>& OutColumns, TArray<UClass*>& ClassStack, const TArray<FProperty*>& PrefixChain, const FString& ColumnPrefix)
{
	if (!Class)
		return;

	// Prevent cycles in expansion.
	if (ClassStack.Contains(Class))
		return;

	ClassStack.Add(Class);

	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
			continue;

		const bool bHasColumn = FExportableMetaData::HasCsvColumn(Property);
		const bool bHasExpand = FExportableMetaData::HasCsvExpand(Property);
		if (!bHasColumn && !bHasExpand)
			continue;

		if (bHasColumn)
		{
			const FString ColumnKey = FExportableMetaData::GetCsvColumn(Property);
			FExportColumnInfo ColumnInfo;
			ColumnInfo.PropertyChain = PrefixChain;
			ColumnInfo.PropertyChain.Add(Property);
			ColumnInfo.ColumnName = ColumnPrefix + (!ColumnKey.IsEmpty() ? ColumnKey : Property->GetName());
			OutColumns.Add(MoveTemp(ColumnInfo));
		}

		if (bHasExpand)
		{
			UClass* InnerClass = GetObjectPropertyClass(Property);
			if (!InnerClass)
				continue;

			// Option A (strict): only expand into classes that also opted-in.
			if (!CanExportClass(InnerClass))
				continue;

			FString Prefix = FExportableMetaData::GetCsvPrefix(Property);
			if (Prefix.IsEmpty())
			{
				Prefix = Property->GetName() + TEXT("_");
			}

			TArray<FProperty*> NextChain = PrefixChain;
			NextChain.Add(Property);
			BuildColumnsForClass(InnerClass, OutColumns, ClassStack, NextChain, ColumnPrefix + Prefix);
		}
	}

	if (UClass* SuperClass = Class->GetSuperClass())
	{
		BuildColumnsForClass(SuperClass, OutColumns, ClassStack, PrefixChain, ColumnPrefix);
	}

	ClassStack.Pop();
}

UClass* USheetTableCSVHandler::GetObjectPropertyClass(FProperty* Property)
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

UObject* USheetTableCSVHandler::ResolveObjectPropertyValue(UObject* Container, FProperty* Property, bool bLoadSoft)
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

bool USheetTableCSVHandler::GetColumnValue(UObject* RootObject, const FExportColumnInfo& Column, FString& OutValue)
{
	OutValue.Reset();
	if (!RootObject || Column.PropertyChain.IsEmpty())
		return false;

	UObject* Current = RootObject;
	for (int32 i = 0; i < Column.PropertyChain.Num() - 1; ++i)
	{
		Current = ResolveObjectPropertyValue(Current, Column.PropertyChain[i], true);
		if (!Current)
			return false;
	}

	FProperty* Leaf = Column.PropertyChain.Last();
	const void* ValuePtr = Leaf ? Leaf->ContainerPtrToValuePtr<void>(Current) : nullptr;
	OutValue = PropertyToString(Leaf, reinterpret_cast<const uint8*>(ValuePtr));
	return true;
}

bool USheetTableCSVHandler::SetColumnValue(UObject* RootObject, const FExportColumnInfo& Column, const FString& InValue)
{
	if (!RootObject || Column.PropertyChain.IsEmpty())
		return false;

	UObject* Current = RootObject;
	for (int32 i = 0; i < Column.PropertyChain.Num() - 1; ++i)
	{
		Current = ResolveObjectPropertyValue(Current, Column.PropertyChain[i], true);
		if (!Current)
			return false;
	}

	FProperty* Leaf = Column.PropertyChain.Last();
	void* ValuePtr = Leaf ? Leaf->ContainerPtrToValuePtr<void>(Current) : nullptr;
	const bool bOk = StringToProperty(Leaf, reinterpret_cast<uint8*>(ValuePtr), InValue);
	if (bOk && Current)
	{
		Current->MarkPackageDirty();
		Current->PostEditChange();
	}
	return bOk;
}

bool USheetTableCSVHandler::ApplyCSVRowToObject(UObject* TargetObject, UClass* TargetClass, const TArray<FString>& Headers, const TArray<FString>& Values)
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

	TArray<FExportColumnInfo> Columns;
	GetExportablePropertiesRecursively(TargetClass, Columns);

	const ESheetTableWriteBackScope Scope = USheetTableEditorPluginSettings::Get()->WriteBackScope;

	// Pass 1: leaf columns (no expansion chain)
	for (const FExportColumnInfo& Col : Columns)
	{
		if (Col.PropertyChain.Num() != 1)
			continue;
		const FString* Found = ColumnToValue.Find(Col.ColumnName);
		if (!Found)
			continue;
		SetColumnValue(TargetObject, Col, *Found);
	}

	// Pass 2: expanded columns
	if (Scope == ESheetTableWriteBackScope::RootAndExpanded)
	{
		for (const FExportColumnInfo& Col : Columns)
		{
			if (Col.PropertyChain.Num() <= 1)
				continue;
			const FString* Found = ColumnToValue.Find(Col.ColumnName);
			if (!Found)
				continue;
			SetColumnValue(TargetObject, Col, *Found);
		}
	}

	return true;
}

FString USheetTableCSVHandler::PropertyToString(FProperty* Property, const uint8* PropertyData)
{
	if (!Property || !PropertyData)
		return FString();

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

	return TEXT("");
}

bool USheetTableCSVHandler::StringToProperty(FProperty* Property, uint8* PropertyData, const FString& StringValue)
{
	if (!Property || !PropertyData)
		return false;

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

	return false;
}

FString USheetTableCSVHandler::EscapeCSVString(const FString& Value)
{
	if (Value.Contains(TEXT(",")) || Value.Contains(TEXT("\"")) || Value.Contains(TEXT("\n")) || Value.Contains(TEXT("\r")))
	{
		FString Escaped = Value.Replace(TEXT("\""), TEXT("\"\""));
		return TEXT("\"") + Escaped + TEXT("\"");
	}
	return Value;
}

TArray<FString> USheetTableCSVHandler::ParseCSVLine(const FString& Line)
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

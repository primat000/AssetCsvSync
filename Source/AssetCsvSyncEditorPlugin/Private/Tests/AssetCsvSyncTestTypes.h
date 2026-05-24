// MIT Licensed. Copyright (c) 2026 Olga Taranova
// Test-only reflected types for AssetCsvSync automation tests.
// These types are compiled into the editor module but are never shipped.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AssetCsvSyncTestTypes.generated.h"

// ---------------------------------------------------------------------------
// Flat row — int / FString / float / bool, with CsvId on int field
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FAssetCsvSyncTestFlatRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (CsvColumn = "id", CsvId))
	int32 Id = 0;

	UPROPERTY(EditAnywhere, meta = (CsvColumn = "name"))
	FString Name;

	UPROPERTY(EditAnywhere, meta = (CsvColumn = "score"))
	float Score = 0.f;

	UPROPERTY(EditAnywhere, meta = (CsvColumn = "flag"))
	bool bFlag = false;
};

UCLASS(meta = (CsvExport = "csvtest_flat"))
class UAssetCsvSyncTestFlatAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, meta = (CsvRows))
	TArray<FAssetCsvSyncTestFlatRow> Rows;
};

// ---------------------------------------------------------------------------
// Expand row — nested struct flattened via CsvExpand
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FAssetCsvSyncTestRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float Min = 0.f;

	UPROPERTY(EditAnywhere)
	float Max = 0.f;
};

USTRUCT(BlueprintType)
struct FAssetCsvSyncTestExpandRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (CsvColumn = "id", CsvId))
	int32 Id = 0;

	UPROPERTY(EditAnywhere, meta = (CsvColumn = "name"))
	FString Name;

	// Expands into columns: Range_Min, Range_Max
	UPROPERTY(EditAnywhere, meta = (CsvExpand))
	FAssetCsvSyncTestRange Range;
};

UCLASS(meta = (CsvExport = "csvtest_expand"))
class UAssetCsvSyncTestExpandAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, meta = (CsvRows))
	TArray<FAssetCsvSyncTestExpandRow> Rows;
};

// ---------------------------------------------------------------------------
// Soft pointer expand — TSoftObjectPtr<UItemData> flattened via CsvExpand
// ---------------------------------------------------------------------------

UCLASS(meta = (CsvExport = "csvtest_item"))
class UAssetCsvSyncTestItemData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, meta = (CsvColumn = "id"))
	int32 Id = 0;

	UPROPERTY(EditAnywhere, meta = (CsvColumn = "title"))
	FString Title;
};

USTRUCT(BlueprintType)
struct FAssetCsvSyncTestSoftPtrRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (CsvColumn = "row_id", CsvId))
	int32 RowId = 0;

	// Expands into columns: Icon_id, Icon_title (properties of the referenced asset)
	UPROPERTY(EditAnywhere, meta = (CsvExpand))
	TSoftObjectPtr<UAssetCsvSyncTestItemData> Icon;
};

UCLASS(meta = (CsvExport = "csvtest_softptr"))
class UAssetCsvSyncTestSoftPtrAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, meta = (CsvRows))
	TArray<FAssetCsvSyncTestSoftPtrRow> Rows;
};

// ---------------------------------------------------------------------------
// Array expand — TArray<FName> flattened via CsvExpand
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FAssetCsvSyncTestArrayExpandRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (CsvColumn = "id", CsvId))
	int32 Id = 0;

	// Expands into columns: Tags_0, Tags_1, ...
	UPROPERTY(EditAnywhere, meta = (CsvExpand))
	TArray<FName> Tags;
};

UCLASS(meta = (CsvExport = "csvtest_array"))
class UAssetCsvSyncTestArrayAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, meta = (CsvRows))
	TArray<FAssetCsvSyncTestArrayExpandRow> Rows;
};

// ---------------------------------------------------------------------------
// Hard pointer expand — TObjectPtr<UAssetCsvSyncTestItemData> via CsvExpand
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FAssetCsvSyncTestHardPtrRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (CsvColumn = "row_id", CsvId))
	int32 RowId = 0;

	// Expands into columns: Item_id, Item_title (properties of the referenced asset)
	UPROPERTY(EditAnywhere, meta = (CsvExpand))
	TObjectPtr<UAssetCsvSyncTestItemData> Item;
};

UCLASS(meta = (CsvExport = "csvtest_hardptr"))
class UAssetCsvSyncTestHardPtrAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, meta = (CsvRows))
	TArray<FAssetCsvSyncTestHardPtrRow> Rows;
};

// ---------------------------------------------------------------------------
// Map expand — TMap<FName, int32> flattened via CsvExpand
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FAssetCsvSyncTestMapExpandRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (CsvColumn = "id", CsvId))
	int32 Id = 0;

	// Expands into columns: Damage_Fire, Damage_Ice, ...
	UPROPERTY(EditAnywhere, meta = (CsvExpand))
	TMap<FName, int32> Damage;
};

UCLASS(meta = (CsvExport = "csvtest_map"))
class UAssetCsvSyncTestMapAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, meta = (CsvRows))
	TArray<FAssetCsvSyncTestMapExpandRow> Rows;
};

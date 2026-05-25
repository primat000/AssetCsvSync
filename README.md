# AssetCsvSync

**AssetCsvSync** is a metadata-driven synchronization plugin for Unreal Engine 5.6 that enables bidirectional exchange between `UDataAsset` classes and CSV files.

The system uses reflection and lightweight `meta` tags to define how properties are mapped to CSV columns — no separate config files, no editor setup beyond annotating your C++ classes.

## Overview

AssetCsvSync provides:

- DataAsset → CSV export
- CSV → DataAsset import
- Multi-row mode: one `UDataAsset` with a `TArray` property = full CSV table, each element is one row
- Nested struct, soft-pointer, and array expansion into flat named columns
- `CsvId`-based row matching on import — reordering rows in the spreadsheet does not corrupt data
- Editor panel UI for export and import
- Python scripting API for automation and round-trip testing

## Installation

1. Copy the `SheetTableEditorPlugin` folder into your project's `Plugins/` directory
2. Recompile the project
3. Enable the plugin in **Edit → Plugins → AssetCsvSync**

The plugin is editor-only and does not affect shipping builds.

## Metadata Tags

| Tag | Scope | Description |
|-----|-------|-------------|
| `CsvExport = "name"` | `UCLASS` | Opts the class into CSV sync; the value is used as the default CSV filename in the editor panel |
| `CsvColumn = "col_name"` | `UPROPERTY` | Maps the property to a CSV column with the given name |
| `CsvId` | `UPROPERTY` | Marks the property as the row identifier for import matching |
| `CsvRows` | `UPROPERTY` on `TArray` | Each array element becomes one CSV row |
| `CsvExpand` | `UPROPERTY` | Flattens a nested struct, soft pointer, or array into separate columns |

## Modes

### Multi-row mode (CsvRows)

One `UDataAsset` with a tagged `TArray` = full CSV table. Each array element is one row.

```cpp
USTRUCT(BlueprintType)
struct FWeaponRow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, meta = (CsvColumn = "id", CsvId))
    int32 Id = 0;

    UPROPERTY(EditAnywhere, meta = (CsvColumn = "title"))
    FString Title;

    UPROPERTY(EditAnywhere, meta = (CsvColumn = "power"))
    float Power = 0.f;
};

UCLASS(meta = (CsvExport = "weapon_table"))
class UWeaponTable : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, meta = (CsvRows))
    TArray<FWeaponRow> Items;
};
```

```csv
id,title,power
1,Sword,25.5
2,Bow,15.0
3,Shield,5.0
```

On import, rows are matched by `CsvId`. If no match is found, a new element is appended.

## CsvExpand

`CsvExpand` flattens a nested struct, soft pointer, or array into separate named columns instead of a single serialized cell.

### Nested struct

```cpp
USTRUCT(BlueprintType)
struct FDamageRange
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    float Min = 0.f;

    UPROPERTY(EditAnywhere)
    float Max = 0.f;
};

USTRUCT(BlueprintType)
struct FWeaponRow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, meta = (CsvColumn = "id", CsvId))
    int32 Id = 0;

    UPROPERTY(EditAnywhere, meta = (CsvColumn = "name"))
    FString Name;

    // Produces columns: Damage_Min, Damage_Max
    UPROPERTY(EditAnywhere, meta = (CsvExpand))
    FDamageRange Damage;
};

UCLASS(meta = (CsvExport = "weapon_table"))
class UWeaponTable : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, meta = (CsvRows))
    TArray<FWeaponRow> Rows;
};
```

```csv
id,name,Damage_Min,Damage_Max
1,Sword,20.0,30.0
2,Bow,10.0,20.0
```

Column names are `PropertyName_FieldName` where `FieldName` is the C++ property name as written in the struct.

### Pointer to another asset

`CsvExpand` works with both `TSoftObjectPtr<T>` and `TObjectPtr<T>` / `UObject*`. The referenced class does not need `CsvExport` — only the properties you want in the CSV need `CsvColumn` or `CsvExpand`. Columns are prefixed with the pointer property name.

```cpp
UCLASS()
class UItemData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, meta = (CsvColumn = "id"))
    int32 Id = 0;

    UPROPERTY(EditAnywhere, meta = (CsvColumn = "title"))
    FString Title;
};

USTRUCT(BlueprintType)
struct FWeaponRow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, meta = (CsvColumn = "row_id", CsvId))
    int32 RowId = 0;

    // Produces columns: Icon_id, Icon_title
    UPROPERTY(EditAnywhere, meta = (CsvExpand))
    TSoftObjectPtr<UItemData> Icon;   // or TObjectPtr<UItemData> / UItemData*
};

UCLASS(meta = (CsvExport = "weapon_table"))
class UWeaponTable : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, meta = (CsvRows))
    TArray<FWeaponRow> Rows;
};
```

```csv
row_id,Icon_id,Icon_title
1,42,Sword Icon
```

On import, the values are written into the asset already referenced by the pointer. The pointer itself is not replaced.

### Array

```cpp
USTRUCT(BlueprintType)
struct FWeaponRow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, meta = (CsvColumn = "id", CsvId))
    int32 Id = 0;

    // Produces columns: Tags_0, Tags_1, ...
    UPROPERTY(EditAnywhere, meta = (CsvExpand))
    TArray<FName> Tags;
};

UCLASS(meta = (CsvExport = "weapon_table"))
class UWeaponTable : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, meta = (CsvRows))
    TArray<FWeaponRow> Rows;
};
```

```csv
id,Tags_0,Tags_1
1,Fire,Ice
2,Melee,Ranged
```

On import, the array is resized to fit the number of columns present in the CSV.

### Map

```cpp
USTRUCT(BlueprintType)
struct FWeaponRow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, meta = (CsvColumn = "id", CsvId))
    int32 Id = 0;

    // Produces columns: Damage_Fire, Damage_Ice, ...
    UPROPERTY(EditAnywhere, meta = (CsvExpand))
    TMap<FName, int32> Damage;
};

UCLASS(meta = (CsvExport = "weapon_table"))
class UWeaponTable : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, meta = (CsvRows))
    TArray<FWeaponRow> Rows;
};
```

```csv
id,Damage_Fire,Damage_Ice
1,50,30
2,20,40
```

On import, the map is reconstructed from the columns whose names match `PropertyName_Key`.

## CsvExpand cases

| Property type | Export | Import | Round-trip |
|---------------|--------|--------|------------|
| Struct (`FStructProperty`) | Columns `Name_Field` | Applied per inner field | ✅ |
| Soft pointer (`TSoftObjectPtr`) | Columns from referenced asset | Written into referenced asset | ✅ |
| Array (`TArray`) | Indexed columns `Name_0`, `Name_1`, … | Array resized to fit | ✅ |
| Map (`TMap`) | Key-based columns `Name_Key` | Reconstructed by key | ✅ |
| Set (`TSet`) | Not supported | Not supported | ❌ |

## Editor Panel

Open via **Tools → Asset CSV Sync**.

Select a DataAsset and a CSV file path, then click **Export to CSV** or **Import CSV**.

## License

MIT License. Copyright (c) 2026 Olga Taranova.

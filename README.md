# AssetCsvSync

**AssetCsvSync** is a metadata-driven synchronization plugin for Unreal Engine that enables bidirectional exchange between `UDataAsset` classes and CSV files.

The system uses reflection and lightweight `meta` tags to define how properties are mapped to CSV columns.

## Overview

AssetCsvSync provides:

- CSV → DataAsset import  
- DataAsset → CSV export   
- Fully reflection-based mapping

# Metadata Tags

AssetCsvSync uses three metadata tags to control how `UDataAsset` classes are exported to and imported from CSV.

## `CsvExport`

**Scope:** `UCLASS`  
**Purpose:** Opt-in a class for CSV processing.

## `CsvColumn`

**Scope:** `UPROPERTY`
**Purpose:** Marks a property as a single CSV column (leaf mode).

## `CsvExpand`

**Scope:** `UPROPERTY`
**Purpose:** Expands nested properties into multiple CSV columns.

# Summary table of cases

| Case | Tag | Export | Import | Round-Trip |
|------|-----|--------|--------|------------|
| Any leaf property (primitives, strings, enums, structs, references, containers) | CsvColumn | Single column: `ExportTextItem` (UE Property Text Format) | `ImportText_Direct` | ✔ |
| Struct expand (`FStructProperty`) | CsvExpand | Columns `PropertyName_Inner` (only inner fields with `CsvColumn`) | `ImportText_Direct` applied to each inner field | ✔ |
| SoftObjectPtr expand (`TSoftObjectPtr<UObject>`) | CsvExpand | Columns `PropertyName_Inner` from target (target class must have `CsvExport`) | Imported into target object using matching columns (if target is valid) | ✔* |
| TArray expand | CsvExpand | Columns by index (`PropertyName_0`, `PropertyName_1`, ...) | Imported by index (array resized if needed) | ✔ |
| TMap expand | CsvExpand | Columns by key (`PropertyName_KeyToken`) | Imported by key (key reconstructed from column name) | ✔ |
| TSet expand | CsvExpand | Converted to array and exported as indexed columns | Not supported (import forbidden) | ✖ |

## Editor
<img width="1008" height="607" alt="image" src="https://github.com/user-attachments/assets/53081c21-34fc-4a9f-b6fe-15eb8c796401" />


> ⚠️ This project is currently under active development.
> APIs and behavior may change without notice.

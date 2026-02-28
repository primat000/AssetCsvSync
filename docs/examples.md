### Example 1 — Basic leaf columns (`CsvColumn`)

```cpp
UCLASS(meta=(CsvExport))
class UWeaponData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, meta=(CsvColumn="id"))
    int32 Id = 1;

    UPROPERTY(EditAnywhere, meta=(CsvColumn="title"))
    FString Title = TEXT("Knife");

    UPROPERTY(EditAnywhere, meta=(CsvColumn="power"))
    float Power = 10.0f;

    UPROPERTY(EditAnywhere, meta=(CsvColumn="enabled"))
    bool bEnabled = true;
};
```

### Example Output

| id | title | power | enabled |
| -- | ----- | ----- | ------- |
| 1  | Knife | 10.000000  | true    |

```csv
id,title,power,enabled
1,Knife,10.000000,true
```
### Example 2 — Struct as leaf (CsvColumn on struct)

```cpp
USTRUCT(BlueprintType)
struct FDamageRange
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, meta=(CsvColumn="min"))
    float Min = 0.f;

    UPROPERTY(EditAnywhere, meta=(CsvColumn="max"))
    float Max = 0.f;
};

UCLASS(meta=(CsvExport))
class UWeaponStats : public UDataAsset
{
    GENERATED_BODY()

public:
    // Leaf struct: serialized as a single UE text value in one CSV cell
    UPROPERTY(EditAnywhere, meta=(CsvColumn="damage_range"))
    FDamageRange DamageRange;
};
```
### Example Output

|       damage_range            |
|-------------------------------|
| (Min=5.000000,Max=10.000000)  |

```csv
damage_range
(Min=5.000000,Max=10.000000)
```

### Example 3 — Struct expand (CsvExpand)

```cpp
USTRUCT(BlueprintType)
struct FItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, meta=(CsvColumn="id"))
    int32 Id = 100;

    UPROPERTY(EditAnywhere, meta=(CsvColumn="title"))
    FString Title = TEXT("Starter");
};

UCLASS(meta=(CsvExport))
class UItemRow : public UDataAsset
{
    GENERATED_BODY()

public:
    // Expanded struct: produces multiple columns using "PropertyName.InnerColumn"
    UPROPERTY(EditAnywhere, meta=(CsvExpand))
    FItem Info;
};
```
### Example Output

| Info.id | Info.title |
| ------- | ---------- |
| 100     | Starter    |

```csv
Info.id,Info.title
100,Starter
```

### Example 4 — Soft object expand (CsvExpand on TSoftObjectPtr)

```cpp
UCLASS(meta=(CsvExport))
class UItemData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, meta=(CsvColumn="id"))
    int32 Id = 0;

    UPROPERTY(EditAnywhere, meta=(CsvColumn="title"))
    FString Title;
};

UCLASS(meta=(CsvExport))
class UShopRow : public UDataAsset
{
    GENERATED_BODY()

public:
    // Expands the referenced asset into columns: "Item.id", "Item.title", etc.
    UPROPERTY(EditAnywhere, meta=(CsvExpand))
    TSoftObjectPtr<UItemData> Item;
};
```
### Example Output

| Item.id | Item.title    |
| ------- | ------------- |
| 200     | Health Potion |

```csv
Item.id,Item.title
200,Health Potion
```

### Example 5 — Array expand (CsvExpand on TArray)

```cpp
UCLASS(meta=(CsvExport))
class UWaveRow : public UDataAsset
{
    GENERATED_BODY()

public:
    // Array expand: indexed columns (Enemies_0, Enemies_1, ...)
    UPROPERTY(EditAnywhere, meta=(CsvExpand))
    TArray<int32> Enemies;
};
```
### Example Output

| Enemies_0 | Enemies_1 |
| --------- | --------- |
| 3         | 5         |

```csv
Enemies.0,Enemies.1
3,5
```

### Example 6 — Map expand (CsvExpand on TMap)

```cpp
UCLASS(meta=(CsvExport))
class UStatsRow : public UDataAsset
{
    GENERATED_BODY()

public:
    // Map expand: key-based columns (Stats_HP, Stats_MP, ...)
    UPROPERTY(EditAnywhere, meta=(CsvExpand))
    TMap<FName, int32> Stats;
};
```
### Example Output

| Stats_HP | Stats_MP |
| -------- | -------- |
| 10       | 4        |

```csv
Stats_HP,Stats_MP
10,4
```

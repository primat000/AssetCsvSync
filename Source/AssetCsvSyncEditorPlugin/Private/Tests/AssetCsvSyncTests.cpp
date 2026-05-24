// MIT Licensed. Copyright (c) 2026 Olga Taranova

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AssetCsvSyncCSVHandler.h"
#include "Tests/AssetCsvSyncTestTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// ============================================================================
// Internal helpers
// ============================================================================

namespace AssetCsvSyncTestHelper
{
	static FString TempFile(const FString& Filename)
	{
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("AssetCsvSyncTests");
		IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);
		return Dir / Filename;
	}
}

// ============================================================================
// ParseCSVLine — plain cells
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_ParseLine_SimpleCells,
	"AssetCsvSync.CSV.ParseLine.SimpleCells",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_ParseLine_SimpleCells::RunTest(const FString& Parameters)
{
	const TArray<FString> R = UAssetCsvSyncCSVHandler::ParseCSVLine(TEXT("alpha,beta,gamma"));
	TestEqual(TEXT("3 cells"), R.Num(), 3);
	if (R.Num() == 3)
	{
		TestEqual(TEXT("[0]"), R[0], FString(TEXT("alpha")));
		TestEqual(TEXT("[1]"), R[1], FString(TEXT("beta")));
		TestEqual(TEXT("[2]"), R[2], FString(TEXT("gamma")));
	}
	return true;
}

// ============================================================================
// ParseCSVLine — quoted field containing a comma
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_ParseLine_QuotedField,
	"AssetCsvSync.CSV.ParseLine.QuotedField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_ParseLine_QuotedField::RunTest(const FString& Parameters)
{
	const TArray<FString> R = UAssetCsvSyncCSVHandler::ParseCSVLine(TEXT("\"hello, world\",foo"));
	TestEqual(TEXT("2 cells"), R.Num(), 2);
	if (R.Num() == 2)
	{
		TestEqual(TEXT("[0]"), R[0], FString(TEXT("hello, world")));
		TestEqual(TEXT("[1]"), R[1], FString(TEXT("foo")));
	}
	return true;
}

// ============================================================================
// ParseCSVLine — escaped quotes inside a quoted field ("" → ")
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_ParseLine_EscapedQuotes,
	"AssetCsvSync.CSV.ParseLine.EscapedQuotes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_ParseLine_EscapedQuotes::RunTest(const FString& Parameters)
{
	// Input: "say ""hi""",bar
	const TArray<FString> R = UAssetCsvSyncCSVHandler::ParseCSVLine(TEXT("\"say \"\"hi\"\"\",bar"));
	TestEqual(TEXT("2 cells"), R.Num(), 2);
	if (R.Num() == 2)
	{
		TestEqual(TEXT("[0]"), R[0], FString(TEXT("say \"hi\"")));
		TestEqual(TEXT("[1]"), R[1], FString(TEXT("bar")));
	}
	return true;
}

// ============================================================================
// ParseCSVLine — whitespace trimmed for unquoted, preserved for quoted
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_ParseLine_WhitespaceTrimming,
	"AssetCsvSync.CSV.ParseLine.WhitespaceTrimming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_ParseLine_WhitespaceTrimming::RunTest(const FString& Parameters)
{
	const TArray<FString> R = UAssetCsvSyncCSVHandler::ParseCSVLine(TEXT("  alpha  ,\"  beta  \",  gamma"));
	TestEqual(TEXT("3 cells"), R.Num(), 3);
	if (R.Num() == 3)
	{
		TestEqual(TEXT("[0] trimmed"),    R[0], FString(TEXT("alpha")));
		TestEqual(TEXT("[1] preserved"),  R[1], FString(TEXT("  beta  ")));
		TestEqual(TEXT("[2] trimmed"),    R[2], FString(TEXT("gamma")));
	}
	return true;
}

// ============================================================================
// ParseCSVLine — empty cells
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_ParseLine_EmptyCells,
	"AssetCsvSync.CSV.ParseLine.EmptyCells",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_ParseLine_EmptyCells::RunTest(const FString& Parameters)
{
	{
		// Two adjacent commas → empty middle cell
		const TArray<FString> R = UAssetCsvSyncCSVHandler::ParseCSVLine(TEXT("a,,c"));
		TestEqual(TEXT("3 cells"), R.Num(), 3);
		if (R.Num() == 3)
			TestTrue(TEXT("[1] empty"), R[1].IsEmpty());
	}
	{
		// Leading comma → empty first cell
		const TArray<FString> R = UAssetCsvSyncCSVHandler::ParseCSVLine(TEXT(",b"));
		TestEqual(TEXT("2 cells"), R.Num(), 2);
		if (R.Num() == 2)
			TestTrue(TEXT("[0] empty"), R[0].IsEmpty());
	}
	{
		// Trailing comma → empty last cell
		const TArray<FString> R = UAssetCsvSyncCSVHandler::ParseCSVLine(TEXT("a,"));
		TestEqual(TEXT("2 cells"), R.Num(), 2);
		if (R.Num() == 2)
			TestTrue(TEXT("[1] empty"), R[1].IsEmpty());
	}
	return true;
}

// ============================================================================
// EscapeCSVString + ParseCSVLine round-trip
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_EscapeRoundTrip,
	"AssetCsvSync.CSV.EscapeRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_EscapeRoundTrip::RunTest(const FString& Parameters)
{
	const TArray<FString> Inputs = {
		TEXT("plain"),
		TEXT("with,comma"),
		TEXT("with\"quote"),
		TEXT("already\"\"doubled"),
		TEXT(""),
		TEXT("all,combined\"value"),
	};

	for (const FString& Original : Inputs)
	{
		const FString Escaped = UAssetCsvSyncCSVHandler::EscapeCSVString(Original);
		const TArray<FString> Parsed = UAssetCsvSyncCSVHandler::ParseCSVLine(Escaped);

		const FString Desc = FString::Printf(TEXT("RoundTrip [%s]"), *Original);
		TestEqual(Desc + TEXT(" cell count"), Parsed.Num(), 1);
		if (Parsed.Num() == 1)
			TestEqual(Desc + TEXT(" value"), Parsed[0], Original);
	}
	return true;
}

// ============================================================================
// Flat rows — full export → import round-trip
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_FlatRows_RoundTrip,
	"AssetCsvSync.Rows.FlatRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_FlatRows_RoundTrip::RunTest(const FString& Parameters)
{
	UAssetCsvSyncTestFlatAsset* Source = NewObject<UAssetCsvSyncTestFlatAsset>(GetTransientPackage());
	Source->Rows = {
		{1, TEXT("Alpha"),             10.5f,  true},
		{2, TEXT("Beta"),               0.0f, false},
		{3, TEXT("Gamma, has comma"),   0.25f,  true},
	};

	const FString Path = AssetCsvSyncTestHelper::TempFile(TEXT("flat_roundtrip.csv"));
	if (!TestTrue(TEXT("Export"), UAssetCsvSyncCSVHandler::ExportDataAssetRowsToCSV(Source, Path)))
		return false;

	UAssetCsvSyncTestFlatAsset* Dest = NewObject<UAssetCsvSyncTestFlatAsset>(GetTransientPackage());
	if (!TestTrue(TEXT("Import"), UAssetCsvSyncCSVHandler::ImportCSVRowsToDataAsset(Path, Dest, false)))
		return false;

	if (!TestEqual(TEXT("Row count"), Dest->Rows.Num(), 3))
		return false;

	TestEqual(TEXT("[0].Id"),   Dest->Rows[0].Id,   1);
	TestEqual(TEXT("[0].Name"), Dest->Rows[0].Name, FString(TEXT("Alpha")));
	TestNearlyEqual(TEXT("[0].Score"), Dest->Rows[0].Score, 10.5f, KINDA_SMALL_NUMBER);
	TestTrue(TEXT("[0].bFlag"),  Dest->Rows[0].bFlag);

	TestEqual(TEXT("[1].Id"),   Dest->Rows[1].Id,   2);
	TestFalse(TEXT("[1].bFlag"), Dest->Rows[1].bFlag);
	TestNearlyEqual(TEXT("[1].Score"), Dest->Rows[1].Score, 0.0f, KINDA_SMALL_NUMBER);

	TestEqual(TEXT("[2].Name"), Dest->Rows[2].Name, FString(TEXT("Gamma, has comma")));
	TestNearlyEqual(TEXT("[2].Score"), Dest->Rows[2].Score, 0.25f, KINDA_SMALL_NUMBER);

	return true;
}

// ============================================================================
// Expand rows — CsvExpand nested struct round-trip
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_ExpandRows_RoundTrip,
	"AssetCsvSync.Rows.ExpandRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_ExpandRows_RoundTrip::RunTest(const FString& Parameters)
{
	UAssetCsvSyncTestExpandAsset* Source = NewObject<UAssetCsvSyncTestExpandAsset>(GetTransientPackage());

	FAssetCsvSyncTestExpandRow Row1;
	Row1.Id = 1; Row1.Name = TEXT("Sword"); Row1.Range.Min = 20.f; Row1.Range.Max = 30.f;

	FAssetCsvSyncTestExpandRow Row2;
	Row2.Id = 2; Row2.Name = TEXT("Bow");   Row2.Range.Min = 10.f; Row2.Range.Max = 20.f;

	Source->Rows = {Row1, Row2};

	const FString Path = AssetCsvSyncTestHelper::TempFile(TEXT("expand_roundtrip.csv"));
	if (!TestTrue(TEXT("Export"), UAssetCsvSyncCSVHandler::ExportDataAssetRowsToCSV(Source, Path)))
		return false;

	// Verify the header contains the expanded column names, not a bare "Range" column
	TArray<FString> Columns;
	UAssetCsvSyncCSVHandler::GetCSVHeaderColumns(Path, Columns);
	TestTrue(TEXT("Header has Range_Min"), Columns.Contains(TEXT("Range_Min")));
	TestTrue(TEXT("Header has Range_Max"), Columns.Contains(TEXT("Range_Max")));
	TestFalse(TEXT("No bare Range column"), Columns.Contains(TEXT("Range")));

	UAssetCsvSyncTestExpandAsset* Dest = NewObject<UAssetCsvSyncTestExpandAsset>(GetTransientPackage());
	if (!TestTrue(TEXT("Import"), UAssetCsvSyncCSVHandler::ImportCSVRowsToDataAsset(Path, Dest, false)))
		return false;

	if (!TestEqual(TEXT("Row count"), Dest->Rows.Num(), 2))
		return false;

	TestNearlyEqual(TEXT("[0].Range.Min"), Dest->Rows[0].Range.Min, 20.f, KINDA_SMALL_NUMBER);
	TestNearlyEqual(TEXT("[0].Range.Max"), Dest->Rows[0].Range.Max, 30.f, KINDA_SMALL_NUMBER);
	TestNearlyEqual(TEXT("[1].Range.Min"), Dest->Rows[1].Range.Min, 10.f, KINDA_SMALL_NUMBER);
	TestNearlyEqual(TEXT("[1].Range.Max"), Dest->Rows[1].Range.Max, 20.f, KINDA_SMALL_NUMBER);

	return true;
}

// ============================================================================
// CsvId matching — rows matched by id, not by position
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_CsvId_Matching,
	"AssetCsvSync.Rows.CsvIdMatching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_CsvId_Matching::RunTest(const FString& Parameters)
{
	// Asset starts with rows 1, 2, 3 in that order.
	UAssetCsvSyncTestFlatAsset* Asset = NewObject<UAssetCsvSyncTestFlatAsset>(GetTransientPackage());
	Asset->Rows = {
		{1, TEXT("One"),   1.f, false},
		{2, TEXT("Two"),   2.f, false},
		{3, TEXT("Three"), 3.f, false},
	};

	// CSV has rows in reverse order; row id=1 has a different name.
	const FString Path = AssetCsvSyncTestHelper::TempFile(TEXT("csvid_match.csv"));
	const FString CSV = TEXT("id,name,score,flag\n3,Three,3,false\n1,One_updated,1,false\n2,Two,2,false\n");
	FFileHelper::SaveStringToFile(CSV, *Path);

	if (!TestTrue(TEXT("Import"), UAssetCsvSyncCSVHandler::ImportCSVRowsToDataAsset(Path, Asset, false)))
		return false;

	// No new rows should have been appended — all three were matched by id.
	TestEqual(TEXT("Row count unchanged"), Asset->Rows.Num(), 3);

	// Row with id=1 must have its name updated, regardless of its position in the array.
	const FAssetCsvSyncTestFlatRow* Row1 = Asset->Rows.FindByPredicate(
		[](const FAssetCsvSyncTestFlatRow& R){ return R.Id == 1; });

	if (TestNotNull(TEXT("Row id=1 exists"), Row1))
		TestEqual(TEXT("Row id=1 name updated"), Row1->Name, FString(TEXT("One_updated")));

	return true;
}

// ============================================================================
// Default values are exported — not skipped when equal to CDO
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_DefaultValues_Exported,
	"AssetCsvSync.Rows.DefaultValuesExported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_DefaultValues_Exported::RunTest(const FString& Parameters)
{
	// One row with all fields at their default (zero) values.
	UAssetCsvSyncTestFlatAsset* Asset = NewObject<UAssetCsvSyncTestFlatAsset>(GetTransientPackage());
	Asset->Rows.Add(FAssetCsvSyncTestFlatRow()); // Id=0, Name="", Score=0.f, bFlag=false

	const FString Path = AssetCsvSyncTestHelper::TempFile(TEXT("default_values.csv"));
	if (!TestTrue(TEXT("Export"), UAssetCsvSyncCSVHandler::ExportDataAssetRowsToCSV(Asset, Path)))
		return false;

	FString Content;
	FFileHelper::LoadFileToString(Content, *Path);
	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines, false);

	// ParseIntoArrayLines keeps the empty string after a trailing '\n' — remove it,
	// mirroring what ImportCSVRowsToDataAsset does.
	Lines.RemoveAll([](const FString& L){ return L.TrimStartAndEnd().IsEmpty(); });

	// Must produce header + exactly one data row.
	if (!TestEqual(TEXT("Line count (header + 1 row)"), Lines.Num(), 2))
		return false;

	const TArray<FString> Headers = UAssetCsvSyncCSVHandler::ParseCSVLine(Lines[0]);
	const TArray<FString> Values  = UAssetCsvSyncCSVHandler::ParseCSVLine(Lines[1]);
	TestEqual(TEXT("Value count matches header"), Values.Num(), Headers.Num());

	auto CheckCell = [&](const FString& ColName, const FString& Expected)
	{
		const int32 Idx = Headers.IndexOfByKey(ColName);
		if (TestTrue(FString::Printf(TEXT("Column '%s' present"), *ColName), Idx != INDEX_NONE)
			&& Idx < Values.Num())
		{
			TestEqual(FString::Printf(TEXT("Column '%s' value"), *ColName), Values[Idx], Expected);
		}
	};

	CheckCell(TEXT("id"),    TEXT("0"));
	CheckCell(TEXT("score"), TEXT("0.0"));   // SanitizeFloat(0.f) == "0.0"
	CheckCell(TEXT("flag"),  TEXT("false"));

	return true;
}

// ============================================================================
// Soft pointer expand — CsvExpand on TSoftObjectPtr writes into the referenced asset
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_SoftPtrExpand,
	"AssetCsvSync.Rows.SoftPtrExpand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_SoftPtrExpand::RunTest(const FString& Parameters)
{
	// Source: one row whose Icon soft pointer is set to a live in-memory item asset.
	UAssetCsvSyncTestItemData* SrcItem = NewObject<UAssetCsvSyncTestItemData>(GetTransientPackage());
	SrcItem->Id    = 42;
	SrcItem->Title = TEXT("Sword Icon");

	UAssetCsvSyncTestSoftPtrAsset* Source = NewObject<UAssetCsvSyncTestSoftPtrAsset>(GetTransientPackage());
	FAssetCsvSyncTestSoftPtrRow SrcRow;
	SrcRow.RowId = 1;
	SrcRow.Icon  = SrcItem;
	Source->Rows.Add(SrcRow);

	const FString Path = AssetCsvSyncTestHelper::TempFile(TEXT("softptr_expand.csv"));
	if (!TestTrue(TEXT("Export"), UAssetCsvSyncCSVHandler::ExportDataAssetRowsToCSV(Source, Path)))
		return false;

	// Header must contain the expanded columns, not a bare "Icon" column.
	TArray<FString> Columns;
	UAssetCsvSyncCSVHandler::GetCSVHeaderColumns(Path, Columns);
	TestTrue(TEXT("Header has Icon_id"),    Columns.Contains(TEXT("Icon_id")));
	TestTrue(TEXT("Header has Icon_title"), Columns.Contains(TEXT("Icon_title")));
	TestFalse(TEXT("No bare Icon column"),  Columns.Contains(TEXT("Icon")));

	// Destination: a pre-populated row pointing to a separate (empty) item.
	// Import must write the CSV values INTO that item, not replace the pointer.
	UAssetCsvSyncTestItemData* DstItem = NewObject<UAssetCsvSyncTestItemData>(GetTransientPackage());

	UAssetCsvSyncTestSoftPtrAsset* Dest = NewObject<UAssetCsvSyncTestSoftPtrAsset>(GetTransientPackage());
	FAssetCsvSyncTestSoftPtrRow DstRow;
	DstRow.RowId = 1;   // matches CsvId in the CSV
	DstRow.Icon  = DstItem;
	Dest->Rows.Add(DstRow);

	if (!TestTrue(TEXT("Import"), UAssetCsvSyncCSVHandler::ImportCSVRowsToDataAsset(Path, Dest, false)))
		return false;

	TestEqual(TEXT("DstItem.Id updated"),    DstItem->Id,    42);
	TestEqual(TEXT("DstItem.Title updated"), DstItem->Title, FString(TEXT("Sword Icon")));

	return true;
}

// ============================================================================
// Hard pointer expand — TObjectPtr writes into the referenced asset (already in memory)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_HardPtrExpand,
	"AssetCsvSync.Rows.HardPtrExpand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_HardPtrExpand::RunTest(const FString& Parameters)
{
	// Source: one row whose Item hard pointer is set to a live in-memory item asset.
	UAssetCsvSyncTestItemData* SrcItem = NewObject<UAssetCsvSyncTestItemData>(GetTransientPackage());
	SrcItem->Id    = 42;
	SrcItem->Title = TEXT("Sword Icon");

	UAssetCsvSyncTestHardPtrAsset* Source = NewObject<UAssetCsvSyncTestHardPtrAsset>(GetTransientPackage());
	FAssetCsvSyncTestHardPtrRow SrcRow;
	SrcRow.RowId = 1;
	SrcRow.Item  = SrcItem;
	Source->Rows.Add(SrcRow);

	const FString Path = AssetCsvSyncTestHelper::TempFile(TEXT("hardptr_expand.csv"));
	if (!TestTrue(TEXT("Export"), UAssetCsvSyncCSVHandler::ExportDataAssetRowsToCSV(Source, Path)))
		return false;

	// Header must contain the expanded columns, not a bare "Item" column.
	TArray<FString> Columns;
	UAssetCsvSyncCSVHandler::GetCSVHeaderColumns(Path, Columns);
	TestTrue(TEXT("Header has Item_id"),    Columns.Contains(TEXT("Item_id")));
	TestTrue(TEXT("Header has Item_title"), Columns.Contains(TEXT("Item_title")));
	TestFalse(TEXT("No bare Item column"),  Columns.Contains(TEXT("Item")));

	// Destination: a pre-populated row pointing to a separate (empty) item.
	// Import must write the CSV values INTO that item, not replace the pointer.
	UAssetCsvSyncTestItemData* DstItem = NewObject<UAssetCsvSyncTestItemData>(GetTransientPackage());

	UAssetCsvSyncTestHardPtrAsset* Dest = NewObject<UAssetCsvSyncTestHardPtrAsset>(GetTransientPackage());
	FAssetCsvSyncTestHardPtrRow DstRow;
	DstRow.RowId = 1;   // matches CsvId in the CSV
	DstRow.Item  = DstItem;
	Dest->Rows.Add(DstRow);

	if (!TestTrue(TEXT("Import"), UAssetCsvSyncCSVHandler::ImportCSVRowsToDataAsset(Path, Dest, false)))
		return false;

	TestEqual(TEXT("DstItem.Id updated"),    DstItem->Id,    42);
	TestEqual(TEXT("DstItem.Title updated"), DstItem->Title, FString(TEXT("Sword Icon")));

	return true;
}

// ============================================================================
// Array expand — TArray<FName> produces indexed columns Tags_0, Tags_1, ...
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_ArrayExpand,
	"AssetCsvSync.Rows.ArrayExpand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_ArrayExpand::RunTest(const FString& Parameters)
{
	UAssetCsvSyncTestArrayAsset* Source = NewObject<UAssetCsvSyncTestArrayAsset>(GetTransientPackage());

	FAssetCsvSyncTestArrayExpandRow Row1;
	Row1.Id   = 1;
	Row1.Tags = {TEXT("Fire"), TEXT("Ice")};

	FAssetCsvSyncTestArrayExpandRow Row2;
	Row2.Id   = 2;
	Row2.Tags = {TEXT("Melee"), TEXT("Ranged")};

	Source->Rows = {Row1, Row2};

	const FString Path = AssetCsvSyncTestHelper::TempFile(TEXT("array_expand.csv"));
	if (!TestTrue(TEXT("Export"), UAssetCsvSyncCSVHandler::ExportDataAssetRowsToCSV(Source, Path)))
		return false;

	// Header must have indexed columns, not a bare "Tags" column.
	TArray<FString> Columns;
	UAssetCsvSyncCSVHandler::GetCSVHeaderColumns(Path, Columns);
	TestTrue(TEXT("Header has Tags_0"),     Columns.Contains(TEXT("Tags_0")));
	TestTrue(TEXT("Header has Tags_1"),     Columns.Contains(TEXT("Tags_1")));
	TestFalse(TEXT("No bare Tags column"),  Columns.Contains(TEXT("Tags")));

	UAssetCsvSyncTestArrayAsset* Dest = NewObject<UAssetCsvSyncTestArrayAsset>(GetTransientPackage());
	if (!TestTrue(TEXT("Import"), UAssetCsvSyncCSVHandler::ImportCSVRowsToDataAsset(Path, Dest, false)))
		return false;

	if (!TestEqual(TEXT("Row count"), Dest->Rows.Num(), 2))
		return false;

	TestEqual(TEXT("[0].Tags.Num"), Dest->Rows[0].Tags.Num(), 2);
	if (Dest->Rows[0].Tags.Num() == 2)
	{
		TestEqual(TEXT("[0].Tags[0]"), Dest->Rows[0].Tags[0], FName(TEXT("Fire")));
		TestEqual(TEXT("[0].Tags[1]"), Dest->Rows[0].Tags[1], FName(TEXT("Ice")));
	}

	TestEqual(TEXT("[1].Tags.Num"), Dest->Rows[1].Tags.Num(), 2);
	if (Dest->Rows[1].Tags.Num() == 2)
	{
		TestEqual(TEXT("[1].Tags[0]"), Dest->Rows[1].Tags[0], FName(TEXT("Melee")));
		TestEqual(TEXT("[1].Tags[1]"), Dest->Rows[1].Tags[1], FName(TEXT("Ranged")));
	}

	return true;
}

// ============================================================================
// Map expand — TMap<FName, int32> produces key-based columns Damage_Fire, etc.
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAssetCsvSync_MapExpand,
	"AssetCsvSync.Rows.MapExpand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAssetCsvSync_MapExpand::RunTest(const FString& Parameters)
{
	UAssetCsvSyncTestMapAsset* Source = NewObject<UAssetCsvSyncTestMapAsset>(GetTransientPackage());

	FAssetCsvSyncTestMapExpandRow Row1;
	Row1.Id = 1;
	Row1.Damage.Add(TEXT("Fire"), 50);
	Row1.Damage.Add(TEXT("Ice"),  30);

	FAssetCsvSyncTestMapExpandRow Row2;
	Row2.Id = 2;
	Row2.Damage.Add(TEXT("Fire"), 20);
	Row2.Damage.Add(TEXT("Ice"),  40);

	Source->Rows = {Row1, Row2};

	const FString Path = AssetCsvSyncTestHelper::TempFile(TEXT("map_expand.csv"));
	if (!TestTrue(TEXT("Export"), UAssetCsvSyncCSVHandler::ExportDataAssetRowsToCSV(Source, Path)))
		return false;

	// Header must have key-based columns, not a bare "Damage" column.
	TArray<FString> Columns;
	UAssetCsvSyncCSVHandler::GetCSVHeaderColumns(Path, Columns);
	TestTrue(TEXT("Header has Damage_Fire"),  Columns.Contains(TEXT("Damage_Fire")));
	TestTrue(TEXT("Header has Damage_Ice"),   Columns.Contains(TEXT("Damage_Ice")));
	TestFalse(TEXT("No bare Damage column"),  Columns.Contains(TEXT("Damage")));

	UAssetCsvSyncTestMapAsset* Dest = NewObject<UAssetCsvSyncTestMapAsset>(GetTransientPackage());
	if (!TestTrue(TEXT("Import"), UAssetCsvSyncCSVHandler::ImportCSVRowsToDataAsset(Path, Dest, false)))
		return false;

	if (!TestEqual(TEXT("Row count"), Dest->Rows.Num(), 2))
		return false;

	const int32* Fire0 = Dest->Rows[0].Damage.Find(TEXT("Fire"));
	const int32* Ice0  = Dest->Rows[0].Damage.Find(TEXT("Ice"));
	if (TestNotNull(TEXT("[0] has Fire"), Fire0)) TestEqual(TEXT("[0].Fire"), *Fire0, 50);
	if (TestNotNull(TEXT("[0] has Ice"),  Ice0))  TestEqual(TEXT("[0].Ice"),  *Ice0,  30);

	const int32* Fire1 = Dest->Rows[1].Damage.Find(TEXT("Fire"));
	const int32* Ice1  = Dest->Rows[1].Damage.Find(TEXT("Ice"));
	if (TestNotNull(TEXT("[1] has Fire"), Fire1)) TestEqual(TEXT("[1].Fire"), *Fire1, 20);
	if (TestNotNull(TEXT("[1] has Ice"),  Ice1))  TestEqual(TEXT("[1].Ice"),  *Ice1,  40);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

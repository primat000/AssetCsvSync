// MIT Licensed. Copyright (c) 2026 Olga Taranova

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AssetCsvSyncEditorPluginSettings.generated.h"

UENUM(BlueprintType)
enum class EAssetCsvSyncWriteBackScope : uint8
{
	RootOnly UMETA(DisplayName = "Root Only"),
	RootAndExpanded UMETA(DisplayName = "Root And Expanded"),
};

UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "AssetCsvSync Editor"))
class ASSETCSVSYNCEDITORPLUGIN_API UAssetCsvSyncEditorPluginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override
	{
		return TEXT("Project");
	}

	virtual FName GetCategoryName() const override
	{
		return TEXT("Plugins");
	}

	virtual FName GetSectionName() const override
	{
		// Project Settings -> Plugins -> AssetCsvSync Editor
		return TEXT("AssetCsvSync Editor");
	}

	UPROPERTY(EditAnywhere, config, Category = "Import")
	EAssetCsvSyncWriteBackScope WriteBackScope = EAssetCsvSyncWriteBackScope::RootAndExpanded;

	static const UAssetCsvSyncEditorPluginSettings* Get()
	{
		return GetDefault<UAssetCsvSyncEditorPluginSettings>();
	}
};

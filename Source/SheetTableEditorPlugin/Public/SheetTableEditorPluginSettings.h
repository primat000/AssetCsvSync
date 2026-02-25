// MIT Licensed. Copyright (c) 2026 Olga Taranova

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SheetTableEditorPluginSettings.generated.h"

UENUM(BlueprintType)
enum class ESheetTableWriteBackScope : uint8
{
	RootOnly UMETA(DisplayName = "Root Only"),
	RootAndExpanded UMETA(DisplayName = "Root And Expanded"),
};

UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Sheet Table Editor"))
class SHEETTABLEEDITORPLUGIN_API USheetTableEditorPluginSettings : public UDeveloperSettings
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
		// Project Settings -> Plugins -> Sheet Table Editor
		return TEXT("Sheet Table Editor");
	}

	UPROPERTY(EditAnywhere, config, Category = "Import")
	ESheetTableWriteBackScope WriteBackScope = ESheetTableWriteBackScope::RootAndExpanded;

	static const USheetTableEditorPluginSettings* Get()
	{
		return GetDefault<USheetTableEditorPluginSettings>();
	}
};

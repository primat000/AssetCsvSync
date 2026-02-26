// MIT Licensed. Copyright (c) 2026 Olga Taranova

#pragma once

#include "IDetailCustomization.h"

class FAssetCsvSyncCSVExportSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

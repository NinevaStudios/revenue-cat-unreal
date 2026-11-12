// Copyright (c) 2026 Nineva Studios

#pragma once

#include "Modules/ModuleManager.h"

class FRevenueCatModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

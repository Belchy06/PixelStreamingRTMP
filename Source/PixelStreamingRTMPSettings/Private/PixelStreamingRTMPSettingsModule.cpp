// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FPixelStreamingRTMPSettingsModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FPixelStreamingRTMPSettingsModule, PixelStreamingRTMPSettings);

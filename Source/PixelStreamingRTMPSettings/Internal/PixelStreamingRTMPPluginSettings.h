// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "PixelStreamingRTMPPluginSettings.generated.h"

#define UE_API PIXELSTREAMINGRTMPSETTINGS_API

// Config loaded/saved to an .ini file.
// It is also exposed through the plugin settings page in editor.
UCLASS(MinimalAPI, config = Game, defaultconfig, meta = (DisplayName = "PixelStreamingRTMP"))
class UPixelStreamingRTMPPluginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// clang-format off
	static UE_API TAutoConsoleVariable<FString> CVarDefaultStreamKey;
	UPROPERTY(config, EditAnywhere, Category = "PixelStreamingRTMP", meta = (
		DisplayName = "Default Stream Key",
		ToolTip = "The RTMP stream key applied to the default streamer (the publish URL it connects to, e.g. rtmp://host:1935/live/<key>)."
		))
	FString DefaultStreamKey = TEXT("");
	// clang-format on

	// Begin UDeveloperSettings Interface
	UE_API virtual FName GetCategoryName() const override;

#if WITH_EDITOR
	UE_API virtual FText GetSectionText() const override;

	UE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End UDeveloperSettings Interface

	// Begin UObject Interface
	UE_API virtual void PostInitProperties() override;
	// End UObject Interface
};

#undef UE_API

// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingRTMPPluginSettings.h"

TAutoConsoleVariable<FString> UPixelStreamingRTMPPluginSettings::CVarDefaultStreamKey(
	TEXT("PixelStreamingRTMP.DefaultStreamKey"),
	TEXT(""),
	TEXT("The RTMP stream key applied to the default streamer (the publish URL it connects to)."),
	ECVF_Default);

FName UPixelStreamingRTMPPluginSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
FText UPixelStreamingRTMPPluginSettings::GetSectionText() const
{
	return NSLOCTEXT("PixelStreamingRTMP", "PixelStreamingRTMPSettingsSection", "PixelStreamingRTMP");
}

void UPixelStreamingRTMPPluginSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property
		&& PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPixelStreamingRTMPPluginSettings, DefaultStreamKey))
	{
		CVarDefaultStreamKey->Set(*DefaultStreamKey, ECVF_SetByProjectSetting);
	}
}
#endif

void UPixelStreamingRTMPPluginSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// Seed the CVar from the config-loaded property so runtime code can read the value from the CVar.
	CVarDefaultStreamKey->Set(*DefaultStreamKey, ECVF_SetByProjectSetting);
}

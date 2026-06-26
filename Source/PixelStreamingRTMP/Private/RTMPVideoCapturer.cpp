#include "RTMPVideoCapturer.h"

#include "RTMPVideoBufferMultiFormat.h"
#include "PixelStreaming2PluginSettings.h"

namespace UE::PixelStreamingRTMP
{
	TSharedPtr<FRTMPVideoCapturer> FRTMPVideoCapturer::Create(bool bInIsSRGB)
	{
		TSharedPtr<FRTMPVideoCapturer> VideoCapturer = TSharedPtr<FRTMPVideoCapturer>(new FRTMPVideoCapturer(bInIsSRGB));

		if (UPixelStreaming2PluginSettings::FDelegates* Delegates = UPixelStreaming2PluginSettings::Delegates())
		{
			Delegates->OnSimulcastEnabledChanged.AddSP(VideoCapturer.ToSharedRef(), &FRTMPVideoCapturer::OnSimulcastEnabledChanged);
			Delegates->OnCaptureUseFenceChanged.AddSP(VideoCapturer.ToSharedRef(), &FRTMPVideoCapturer::OnCaptureUseFenceChanged);
			Delegates->OnUseMediaCaptureChanged.AddSP(VideoCapturer.ToSharedRef(), &FRTMPVideoCapturer::OnUseMediaCaptureChanged);
		}
		return VideoCapturer;
	}

	FRTMPVideoCapturer::FRTMPVideoCapturer(bool bIsSRGB)
		: FVideoCapturer(bIsSRGB)
	{
		CreateFrameCapturer();
	}
} // namespace UE::PixelStreamingRTMP
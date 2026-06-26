#include "RTMPVideoSource.h"

#include "PixelCaptureBufferFormat.h"

namespace UE::PixelStreamingRTMP
{
	TSharedPtr<FRTMPVideoSource> FRTMPVideoSource::Create(TSharedPtr<FRTMPVideoCapturer> InVideoCapturer, TSharedPtr<FVideoSourceGroup> InVideoSourceGroup)
	{
		TSharedPtr<FRTMPVideoSource> VideoSource = TSharedPtr<FRTMPVideoSource>(new FRTMPVideoSource(InVideoCapturer));

		InVideoSourceGroup->AddVideoSource(VideoSource);

		return VideoSource;
	}

	FRTMPVideoSource::FRTMPVideoSource(TSharedPtr<FRTMPVideoCapturer> InVideoCapturer)
		: VideoCapturer(InVideoCapturer)
	{
	}

	void FRTMPVideoSource::PushFrame()
	{
		OnFramePushed.Broadcast(MakeShared<FRTMPVideoBufferMultiFormat>(VideoCapturer));
	}

	void FRTMPVideoSource::ForceKeyFrame()
	{
	}
} // namespace UE::PixelStreamingRTMP
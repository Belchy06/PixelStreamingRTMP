#pragma once

#include "RTMPVideoBufferMultiFormat.h"
#include "RTMPVideoCapturer.h"
#include "VideoSource.h"
#include "VideoSourceGroup.h"

namespace UE::PixelStreamingRTMP
{
	using namespace UE::PixelStreaming2;

	class FRTMPVideoSource : public FVideoSource
	{
	public:
		static TSharedPtr<FRTMPVideoSource> Create(TSharedPtr<FRTMPVideoCapturer> InVideoCapturer, TSharedPtr<FVideoSourceGroup> InVideoSourceGroup);
		virtual ~FRTMPVideoSource() = default;

		virtual void PushFrame() override;
		virtual void ForceKeyFrame() override;

		DECLARE_MULTICAST_DELEGATE_OneParam(FOnFramePushed, TSharedPtr<FRTMPVideoBufferMultiFormat>);
		FOnFramePushed OnFramePushed;
	private:
		FRTMPVideoSource(TSharedPtr<FRTMPVideoCapturer> InVideoCapturer);

	private:
		TSharedPtr<FRTMPVideoCapturer> VideoCapturer;
	};
} // namespace UE::PixelStreamingRTMP
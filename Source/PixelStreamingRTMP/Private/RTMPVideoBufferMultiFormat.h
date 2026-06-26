#pragma once

#include "IPixelCaptureOutputFrame.h"
#include "PixelCaptureBufferFormat.h"
#include "RTMPVideoCapturer.h"

namespace UE::PixelStreamingRTMP
{
	class FRTMPVideoBufferMultiFormat
	{
	public:
		FRTMPVideoBufferMultiFormat(TSharedPtr<FRTMPVideoCapturer> Capturer);

		TSharedPtr<IPixelCaptureOutputFrame> RequestFormat(int32 Format);

	private:
		TSharedPtr<FRTMPVideoCapturer> Capturer;
	};
} // namespace UE::PixelStreamingRTMP
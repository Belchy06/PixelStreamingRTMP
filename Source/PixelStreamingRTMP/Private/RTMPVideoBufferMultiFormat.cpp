#include "RTMPVideoBufferMultiFormat.h"

namespace UE::PixelStreamingRTMP
{
	FRTMPVideoBufferMultiFormat::FRTMPVideoBufferMultiFormat(TSharedPtr<FRTMPVideoCapturer> Capturer)
		: Capturer(Capturer)
	{
	}

	TSharedPtr<IPixelCaptureOutputFrame> FRTMPVideoBufferMultiFormat::RequestFormat(int32 Format)
	{
		return Capturer->RequestFormat(Format);
	}
} // namespace UE::PixelStreamingRTMP
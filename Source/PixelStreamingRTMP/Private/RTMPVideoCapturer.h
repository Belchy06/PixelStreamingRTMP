#pragma once

#include "VideoCapturer.h"

namespace UE::PixelStreamingRTMP
{
    using namespace UE::PixelStreaming2;

	class FRTMPVideoCapturer : public FVideoCapturer
	{
	public:
		static TSharedPtr<FRTMPVideoCapturer> Create(bool bIsSRGB = false);
		virtual ~FRTMPVideoCapturer() = default;

	private:
		FRTMPVideoCapturer(bool bIsSRGB);
	};
} // namespace UE::PixelStreamingRTMP
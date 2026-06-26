#pragma once

#include "RTMPVideoBufferMultiFormat.h"
#include "Video/Encoders/Configs/VideoEncoderConfigAV1.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH264.h"
#include "Video/Encoders/Configs/VideoEncoderConfigVP8.h"
#include "Video/Encoders/Configs/VideoEncoderConfigVP9.h"
#include "Video/Resources/VideoResourceCPU.h"
#include "Video/Resources/VideoResourceRHI.h"
#include "Video/VideoConfig.h"
#include "Video/VideoEncoder.h"

namespace UE::PixelStreamingRTMP
{
	TSharedPtr<class IRTMPVideoEncoder> CreateVideoEncoder(EVideoCodec Codec);

	class IRTMPVideoEncoder
	{
	public:
		virtual ~IRTMPVideoEncoder() = default;

		virtual void Encode(TSharedPtr<FRTMPVideoBufferMultiFormat> Frame) = 0;

		DECLARE_TS_MULTICAST_DELEGATE_ThreeParams(FOnEncodedVideo, FVideoPacket&, int32, int32)
		FOnEncodedVideo OnEncodedVideo;
	};

	template <std::derived_from<FVideoResource> TVideoResource>
	class TRTMPVideoEncoder : public IRTMPVideoEncoder
	{
	public:
		TRTMPVideoEncoder(EVideoCodec Codec);
		virtual ~TRTMPVideoEncoder() = default;

		virtual void Encode(TSharedPtr<FRTMPVideoBufferMultiFormat> Frame) override;

	private:
		void UpdateConfig(uint32 Width, uint32 Height);

	private:
		TSharedPtr<TVideoEncoder<TVideoResource>> Encoder;

		EVideoCodec Codec;
	};
} // namespace UE::PixelStreamingRTMP
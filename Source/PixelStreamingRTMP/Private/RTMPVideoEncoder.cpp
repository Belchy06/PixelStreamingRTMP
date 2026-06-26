#include "RTMPVideoEncoder.h"

#include "Logging.h"
#include "PixelCaptureOutputFrameI420.h"
#include "PixelCaptureOutputFrameRHI.h"
#include "PixelStreaming2PluginSettings.h"
#include "UtilsCoder.h"
#include "UtilsVideo.h"

namespace
{
	template <typename TConfig>
	UE::PixelStreamingRTMP::IRTMPVideoEncoder* CreateEncoder(EVideoCodec Codec)
	{
		UE::PixelStreamingRTMP::IRTMPVideoEncoder* Encoder = nullptr;

		if (UE::PixelStreaming2::IsHardwareEncoderSupported<TConfig>())
		{
			Encoder = new UE::PixelStreamingRTMP::TRTMPVideoEncoder<FVideoResourceRHI>(Codec);
		}
		else if (UE::PixelStreaming2::IsSoftwareEncoderSupported<TConfig>())
		{
			Encoder = new UE::PixelStreamingRTMP::TRTMPVideoEncoder<FVideoResourceCPU>(Codec);
		}

		return Encoder;
	}

	// Explicit initialisation. We don't want this util namespace public
	template UE::PixelStreamingRTMP::IRTMPVideoEncoder* CreateEncoder<FVideoEncoderConfigH264>(EVideoCodec Codec);
	template UE::PixelStreamingRTMP::IRTMPVideoEncoder* CreateEncoder<FVideoEncoderConfigAV1>(EVideoCodec Codec);
	template UE::PixelStreamingRTMP::IRTMPVideoEncoder* CreateEncoder<FVideoEncoderConfigVP8>(EVideoCodec Codec);
	template UE::PixelStreamingRTMP::IRTMPVideoEncoder* CreateEncoder<FVideoEncoderConfigVP9>(EVideoCodec Codec);
} // namespace

namespace UE::PixelStreamingRTMP
{
	TSharedPtr<class IRTMPVideoEncoder> CreateVideoEncoder(EVideoCodec Codec)
	{
		IRTMPVideoEncoder* Encoder = nullptr;

		switch (Codec)
		{
			case EVideoCodec::H264:
				Encoder = ::CreateEncoder<FVideoEncoderConfigH264>(Codec);
				break;
			case EVideoCodec::AV1:
				Encoder = ::CreateEncoder<FVideoEncoderConfigAV1>(Codec);
				break;
			case EVideoCodec::VP8:
				Encoder = ::CreateEncoder<FVideoEncoderConfigVP8>(Codec);
				break;
			case EVideoCodec::VP9:
				Encoder = ::CreateEncoder<FVideoEncoderConfigVP9>(Codec);
				break;
			default:
				// Every codec should be accounted for
				checkNoEntry();
		}

		if (!Encoder)
		{
			UE_LOGFMT(LogPixelStreamingRTMP, Error, "Failed to create encoder!");
			return nullptr;
		}

		return TSharedPtr<IRTMPVideoEncoder>(Encoder);
	}

	template <std::derived_from<FVideoResource> TVideoResource>
	TRTMPVideoEncoder<TVideoResource>::TRTMPVideoEncoder(EVideoCodec Codec)
		: Codec(Codec)
	{
		switch (Codec)
		{
			case EVideoCodec::H264:
			{
				FVideoEncoderConfigH264 VideoConfig;
				VideoConfig.RepeatSPSPPS = true;
				VideoConfig.Preset = EAVPreset::UltraLowQuality;
				VideoConfig.LatencyMode = EAVLatencyMode::UltraLowLatency;
				// We set width and height to zero here because we initialize encoder from the first frame dimensions, not this config.
				VideoConfig.Width = 0;
				VideoConfig.Height = 0;
				VideoConfig.TargetFramerate = 60;
				VideoConfig.TargetBitrate = 2500000;
				VideoConfig.MaxBitrate = 5000000;
				VideoConfig.MinQuality = 0;
				VideoConfig.MaxQuality = 100;
				VideoConfig.RateControlMode = ERateControlMode::CBR;
				VideoConfig.bFillData = false;
				VideoConfig.KeyframeInterval = 300;
				VideoConfig.MultipassMode = EMultipassMode::Full;

				Encoder = FVideoEncoder::Create<TVideoResource>(FAVDevice::GetHardwareDevice(), VideoConfig);
				if (!Encoder)
				{
					UE_LOGFMT(LogPixelStreamingRTMP, Error, "PixelStreamingRMTPVideoEncoder: Unable to get or create H264 Encoder");
				}
				break;
			}
			case EVideoCodec::AV1:
			{
				FVideoEncoderConfigAV1 VideoConfig;
				Encoder = FVideoEncoder::Create<TVideoResource>(FAVDevice::GetHardwareDevice(), VideoConfig);
				if (!Encoder)
				{
					UE_LOGFMT(LogPixelStreamingRTMP, Error, "PixelStreamingRMTPVideoEncoder: Unable to get or create AV1 Encoder");
				}
				break;
			}
			case EVideoCodec::VP8:
			{
				FVideoEncoderConfigVP8 VideoConfig;
				Encoder = FVideoEncoder::Create<TVideoResource>(FAVDevice::GetHardwareDevice(), VideoConfig);
				if (!Encoder)
				{
					UE_LOGFMT(LogPixelStreamingRTMP, Error, "PixelStreamingRMTPVideoEncoder: Unable to get or create VP8 Encoder");
				}
				break;
			}
			case EVideoCodec::VP9:
			{
				FVideoEncoderConfigVP9 VideoConfig;
				Encoder = FVideoEncoder::Create<TVideoResource>(FAVDevice::GetHardwareDevice(), VideoConfig);
				if (!Encoder)
				{
					UE_LOGFMT(LogPixelStreamingRTMP, Error, "PixelStreamingRMTPVideoEncoder: Unable to get or create VP9 Encoder");
				}
				break;
			}
			default:
				checkNoEntry();
		}
	}

	template <std::derived_from<FVideoResource> TVideoResource>
	void TRTMPVideoEncoder<TVideoResource>::Encode(TSharedPtr<FRTMPVideoBufferMultiFormat> Frame)
	{
		TSharedPtr<IPixelCaptureOutputFrame> AdaptedLayer;
		if constexpr (std::is_same_v<TVideoResource, FVideoResourceRHI>)
		{
			AdaptedLayer = Frame->RequestFormat(PixelCaptureBufferFormat::FORMAT_RHI);
		}
		else if constexpr (std::is_same_v<TVideoResource, FVideoResourceCPU>)
		{
			AdaptedLayer = Frame->RequestFormat(PixelCaptureBufferFormat::FORMAT_I420);
		}
		else
		{
			UE_LOGFMT(LogPixelStreamingRTMP, Error, "VideoResource isn't a compatible type! Expected either a FVideoResourceRHI or FVideoResourceCPU. Received: {0}", UE_STRINGIZE(TVideoResource));
			return;
		}

		if (!AdaptedLayer)
		{
			return;
		}

		int32 FrameWidth;
		int32 FrameHeight;

		TSharedPtr<TVideoResource> VideoResource;
		if constexpr (std::is_same_v<TVideoResource, FVideoResourceRHI>)
		{
			const FPixelCaptureOutputFrameRHI& RHILayer = StaticCast<const FPixelCaptureOutputFrameRHI&>(*AdaptedLayer.Get());
			// Ensure we have a texture. Some capturers (eg mediacapture), can return frames with no texture while it's initializing
			if (RHILayer.GetFrameTexture() == nullptr)
			{
				return;
			}

			FrameWidth = AdaptedLayer->GetWidth();
			FrameHeight = AdaptedLayer->GetHeight();

			VideoResource = MakeShared<FVideoResourceRHI>(
				Encoder->GetDevice().ToSharedRef(),
				FVideoResourceRHI::FRawData{ RHILayer.GetFrameTexture(), nullptr, 0 });
		}
		else if constexpr (std::is_same_v<TVideoResource, FVideoResourceCPU>)
		{
			const FPixelCaptureOutputFrameI420& CPULayer = StaticCast<const FPixelCaptureOutputFrameI420&>(*AdaptedLayer.Get());
			// Ensure we have a texture. Some capturers (eg mediacapture), can return frames with no texture while it's initializing
			if (CPULayer.GetI420Buffer() == nullptr)
			{
				return;
			}

			FrameWidth = AdaptedLayer->GetWidth();
			FrameHeight = AdaptedLayer->GetHeight();

			VideoResource = MakeShared<FVideoResourceCPU>(
				Encoder->GetDevice().ToSharedRef(),
				MakeShareable<uint8>(CPULayer.GetI420Buffer()->GetMutableData(), UE::PixelStreaming2::TFakeDeleter<uint8>()),
				FAVLayout(CPULayer.GetI420Buffer()->GetStrideY(), 0, CPULayer.GetI420Buffer()->GetSize()),
				FVideoDescriptor(EVideoFormat::YUV420, FrameWidth, FrameHeight));
		}
		else
		{
			UE_LOGFMT(LogPixelStreamingRTMP, Error, "VideoResource isn't a compatible type! Expected either a FVideoResourceRHI or FVideoResourceCPU. Received: {0}", UE_STRINGIZE(TVideoResource));
			return;
		}

		// Update the encoding config using the incoming frame resolution
		UpdateConfig(FrameWidth, FrameHeight);

		uint64_t  Timestamp = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64());
		FAVResult Result = Encoder->SendFrame(VideoResource, Timestamp, false /* bIsKeyframe */);
		if (Result.IsNotSuccess())
		{
			UE_LOGFMT(LogPixelStreamingRTMP, Warning, "TRTMPVideoEncoder. SendFrame failed: {0}", Result.Message);
		}

		FVideoPacket Packet;
		while (Encoder->ReceivePacket(Packet))
		{
			OnEncodedVideo.Broadcast(Packet, FrameWidth, FrameHeight);
		}
	}

	template <std::derived_from<FVideoResource> TVideoResource>
	void TRTMPVideoEncoder<TVideoResource>::UpdateConfig(uint32 Width, uint32 Height)
	{
		if (!Encoder)
		{
			return;
		}

		FVideoEncoderConfig	 VideoConfigMinimal = Encoder->GetMinimalConfig();
		FVideoEncoderConfig* VideoConfig = &VideoConfigMinimal;

		switch (Codec)
		{
			case EVideoCodec::H264:
			{
				if (Encoder->GetInstance()->template Has<FVideoEncoderConfigH264>())
				{
					FVideoEncoderConfigH264& VideoConfigH264 = Encoder->GetInstance()->template Edit<FVideoEncoderConfigH264>();
					VideoConfig = &VideoConfigH264;

					VideoConfigH264.Profile = UE::PixelStreaming2::GetEnumFromCVar<EH264Profile>(UPixelStreaming2PluginSettings::CVarEncoderH264Profile);
				}

				VideoConfig->Width = Width;
				VideoConfig->Height = Height;
				VideoConfig->TargetBitrate = 2500000;
				VideoConfig->MaxBitrate = 5000000;
				VideoConfig->MinQuality = 0;
				VideoConfig->MaxQuality = 100;
				VideoConfig->RateControlMode = ERateControlMode::CBR;
				VideoConfig->MultipassMode = EMultipassMode::Full;
				VideoConfig->bFillData = false;
				break;
			}
			case EVideoCodec::AV1:
			{
				if (Encoder->GetInstance()->template Has<FVideoEncoderConfigAV1>())
				{
					FVideoEncoderConfigAV1& VideoConfigAV1 = Encoder->GetInstance()->template Edit<FVideoEncoderConfigAV1>();
					VideoConfig = &VideoConfigAV1;
				}
				break;
			}
			case EVideoCodec::VP8:
			{
				if (Encoder->GetInstance()->template Has<FVideoEncoderConfigVP8>())
				{
					FVideoEncoderConfigVP8& VideoConfigVP8 = Encoder->GetInstance()->template Edit<FVideoEncoderConfigVP8>();
					VideoConfig = &VideoConfigVP8;
				}
				break;
			}
			case EVideoCodec::VP9:
			{
				if (Encoder->GetInstance()->template Has<FVideoEncoderConfigVP9>())
				{
					FVideoEncoderConfigVP9& VideoConfigVP9 = Encoder->GetInstance()->template Edit<FVideoEncoderConfigVP9>();
					VideoConfig = &VideoConfigVP9;
				}
				break;
			}

			default:
				// We don't support encoders for other codecs
				checkNoEntry();
		}

		Encoder->SetMinimalConfig(*VideoConfig);
	}

	// Explicit specialisation
	template class TRTMPVideoEncoder<FVideoResourceRHI>;
	template class TRTMPVideoEncoder<FVideoResourceCPU>;
} // namespace UE::PixelStreamingRTMP
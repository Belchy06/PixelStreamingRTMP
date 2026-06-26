#pragma once

#include "Audio/Resources/AudioResourceCPU.h"
#include "Audio/AudioEncoder.h"
#include "Audio/AudioPacket.h"
#include "Audio/Encoders/Configs/AudioEncoderConfigAAC.h"
#include "Misc/Optional.h"

namespace UE::PixelStreamingRTMP
{
	TSharedPtr<class IRTMPAudioEncoder> CreateAudioEncoder();

	class IRTMPAudioEncoder
	{
	public:
		virtual ~IRTMPAudioEncoder() = default;

		virtual void Encode(const int16_t* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate) = 0;

		DECLARE_TS_MULTICAST_DELEGATE_ThreeParams(FOnEncodedAudio, FAudioPacket&, int32, const int32)
			FOnEncodedAudio OnEncodedAudio;
	};

	template <std::derived_from<FAudioResource> TAudioResource>
	class TRTMPAudioEncoder : public IRTMPAudioEncoder
	{
	public:
		TRTMPAudioEncoder();
		virtual ~TRTMPAudioEncoder() = default;

		virtual void Encode(const int16_t* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate) override;

	private:
		void UpdateConfig(int32 NumChannels, const int32 SampleRate);

	private:
		TSharedPtr<TAudioEncoder<FAudioResourceCPU>> Encoder;

		// Running count of emitted AAC frames, used to derive a uniform sample-accurate PTS.
		uint64 FrameCount = 0;

		// Wall-clock time (ms) of the first encoded audio, so audio shares a time base with
		// video (DtsOffset is shared between the two in the streamer).
		TOptional<uint64> AudioStartMs;
	};
} // namespace UE::PixelStreamingRTMP
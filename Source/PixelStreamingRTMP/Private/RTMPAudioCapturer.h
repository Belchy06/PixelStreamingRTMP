#pragma once

#include "AudioCapturer.h"

namespace UE::PixelStreamingRTMP
{
	using namespace UE::PixelStreaming2;
	/**
	 * FRTMPAudioCapturer overrides the default PushAudio behaviour of the FAudioCapturer in order to
	 * break up the pushed audio into 10ms chunks
	 */
	class FRTMPAudioCapturer : public FAudioCapturer
	{
	public:
		virtual ~FRTMPAudioCapturer() = default;

		// Override the push audio method as EpicRtc needs the broadcasted audio to be in 10ms chunks
		virtual void PushAudio(const float* AudioData, int32 NumSamples, int32 NumChannels, int32 SampleRate) override;

	protected:
		FRTMPAudioCapturer(const int SampleRate = 48000, const int NumChannels = 2, const float SampleSizeInSeconds = 0.5f)
			: FAudioCapturer(SampleRate, NumChannels, SampleSizeInSeconds) 
		{
		}

	private:
		TArray<int16_t> RecordingBuffer;

		// Needed so FAudioCapturer::Create can access the constructor
		friend FAudioCapturer;
	};
} // namespace UE::PixelStreamingRTMP

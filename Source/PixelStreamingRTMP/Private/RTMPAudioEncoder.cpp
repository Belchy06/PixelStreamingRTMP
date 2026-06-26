#include "RTMPAudioEncoder.h"

#include "Logging.h"
#include "SampleBuffer.h"
#include "Serialization.h"
#include "Serialization/BufferArchive.h"
#include "Templates/SharedPointer.h"
#include "UtilsCore.h"
#include "UtilsVideo.h"

namespace
{
	template <typename TConfig>
	UE::PixelStreamingRTMP::IRTMPAudioEncoder* CreateEncoder()
	{
		UE::PixelStreamingRTMP::IRTMPAudioEncoder* Encoder = nullptr;

		Encoder = new UE::PixelStreamingRTMP::TRTMPAudioEncoder<FAudioResourceCPU>();

		return Encoder;
	}

	// Explicit initialisation. We don't want this util namespace public
	template UE::PixelStreamingRTMP::IRTMPAudioEncoder* CreateEncoder<FAudioEncoderConfigAAC>();
} // namespace

namespace UE::PixelStreamingRTMP
{
	TSharedPtr<class IRTMPAudioEncoder> CreateAudioEncoder()
	{
		IRTMPAudioEncoder* Encoder = nullptr;

		Encoder = ::CreateEncoder<FAudioEncoderConfigAAC>();

		if (!Encoder)
		{
			UE_LOGFMT(LogPixelStreamingRTMP, Error, "Failed to create encoder!");
			return nullptr;
		}

		return TSharedPtr<IRTMPAudioEncoder>(Encoder);
	}

	template <std::derived_from<FAudioResource> TAudioResource>
	TRTMPAudioEncoder<TAudioResource>::TRTMPAudioEncoder()
	{
		FAudioEncoderConfigAAC AudioConfig;
		Encoder = FAudioEncoder::CreateChecked<TAudioResource>(FAVDevice::GetHardwareDevice(), AudioConfig);
	}

	UE_DISABLE_OPTIMIZATION
	template <std::derived_from<FAudioResource> TAudioResource>
	void TRTMPAudioEncoder<TAudioResource>::Encode(const int16_t* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate)
	{
		if (!Encoder)
		{
			return;
		}

		UpdateConfig(NumChannels, SampleRate);

		FAVLayout const		   ResourceLayout = FAVLayout(NumSamples, 0, NumSamples);
		float				   Duration = ((float)NumSamples / (float)SampleRate) / (float)NumChannels;
		FAudioDescriptor const ResourceDescriptor = FAudioDescriptor(NumSamples, Duration);

		Audio::TSampleBuffer<float> Buffer(AudioData, NumSamples, NumChannels, SampleRate);

		TSharedPtr<FAudioResourceCPU> AudioResource = MakeShared<FAudioResourceCPU>(
			Encoder->GetDevice().ToSharedRef(),
			MakeShareable(new float[ResourceLayout.Size]),
			ResourceLayout,
			ResourceDescriptor);

		// The resource holds float samples, so copy the full byte size of the buffer.
		// (ResourceLayout.Size is a sample count, not a byte count — using it here would copy
		// only a quarter of the data and leave the rest uninitialized, producing distortion.)
		FMemory::Memcpy(AudioResource->GetRaw().Get(), Buffer.GetArrayView().GetData(), NumSamples * sizeof(float));

		uint64_t Timestamp = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64());

		// Anchor audio to the wall clock once, so its timeline shares a base with video.
		if (!AudioStartMs.IsSet())
		{
			AudioStartMs = Timestamp;
		}

		FAVResult Result = Encoder->SendFrame(AudioResource, Timestamp);
		if (Result.IsNotSuccess())
		{
			UE_LOGFMT(LogPixelStreamingRTMP, Warning, "TRTMPAudioEncoder. SendFrame failed: {0}", Result.Message);
		}

		// Broadcast each encoded AAC frame on its own. Each frame is a complete raw AAC access
		// unit and must map to exactly one FLV AUDIODATA tag — concatenating frames into a
		// single packet produces an undecodable stream.
		FAudioPacket Packet;
		while (Encoder->ReceivePacket(Packet))
		{
			// AAC-LC emits exactly 1024 samples per frame, so derive a uniform PTS from the
			// frame count instead of the (jittery) wall-clock drain time. Input arrives in
			// 10ms chunks but frames are ~21.3ms, so drains are irregular; tagging frames with
			// capture time would make the timestamps non-uniform and stutter on playback.
			Packet.Timestamp = AudioStartMs.GetValue() + (FrameCount * 1024ull * 1000ull) / SampleRate;
			++FrameCount;

			OnEncodedAudio.Broadcast(Packet, NumChannels, SampleRate);
		}
	}

	template <std::derived_from<FAudioResource> TAudioResource>
	void TRTMPAudioEncoder<TAudioResource>::UpdateConfig(int32 NumChannels, const int32 SampleRate)
	{
		if (!Encoder)
		{
			return;
		}

		FAudioEncoderConfig	 AudioConfigMinimal = Encoder->GetMinimalConfig();
		FAudioEncoderConfig* AudioConfig = &AudioConfigMinimal;

		AudioConfig->NumChannels = NumChannels;
		AudioConfig->Samplerate = SampleRate;

		Encoder->SetMinimalConfig(*AudioConfig);
	}
	UE_ENABLE_OPTIMIZATION

	// Explicit specialisation
	template class TRTMPAudioEncoder<FAudioResourceCPU>;
} // namespace UE::PixelStreamingRTMP
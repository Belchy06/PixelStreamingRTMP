#include "Streamer.h"

#include "Logging.h"
#include "PixelStreamingRTMPPluginSettings.h"
#include "UtilsAAC.h"
#include "UtilsAVC.h"
#include "VideoProducerBackBuffer.h"
#include "VideoProducerMediaCapture.h"
#include "Video/CodecUtils/CodecUtilsH264.h"

namespace UE::PixelStreamingRTMP
{
	using namespace UE::AVCodecCore::H264;

	static const FString RTMPStreamType = TEXT("RTMP");

	namespace
	{
		// Sends one muxed packet on its own chunk stream. Always uses a full (type-0) header
		// with an absolute timestamp so librtmp never derives a cross-packet delta; combined
		// with per-media channels this avoids the corrupt extended-timestamp path that strict
		// servers reject. Timestamps are offset to the stream start and stay under 0xFFFFFF.
		template <typename TPacket>
		void SendRTMPPacket(RTMP* Rtmp, const TPacket& Packet)
		{
			RTMPPacket Out = {};
			if (!RTMPPacket_Alloc(&Out, Packet.BodySize))
			{
				UE_LOGFMT(LogPixelStreamingRTMP, Warning, "Unable to allocate RTMP packet");
				return;
			}

			Out.m_packetType = TPacket::RTMPType;
			Out.m_nChannel = TPacket::RTMPChannel;
			Out.m_headerType = RTMP_PACKET_SIZE_LARGE;
			Out.m_hasAbsTimestamp = 1;
			Out.m_nTimeStamp = Packet.Timestamp;
			Out.m_nInfoField2 = Rtmp->m_stream_id;
			Out.m_nBodySize = Packet.BodySize;
			FMemory::Memcpy(Out.m_body, Packet.Body.Get(), Packet.BodySize);

			if (!RTMP_SendPacket(Rtmp, &Out, 0 /* queue */))
			{
				UE_LOGFMT(LogPixelStreamingRTMP, Warning, "Unable to send packet to server");
			}

			RTMPPacket_Free(&Out);
		}
	} // namespace

	FSendPacketsTask::FSendPacketsTask(TSharedPtr<FRTMP> Stream)
		: Stream(Stream)
	{
	}

	void FSendPacketsTask::Tick(float DeltaMs)
	{
		RTMP* Rtmp = Stream->RtmpPtr;

		FRTMPPacketVariant Packet;
		while (Stream->Packets.Dequeue(Packet))
		{
			// The variant alternative carries its own RTMP type and channel, so dispatch on
			// the type rather than inspecting the bytes.
			Visit([Rtmp](const auto& TypedPacket) { SendRTMPPacket(Rtmp, TypedPacket); }, Packet);
		}
	}

	const FString& FSendPacketsTask::GetName() const
	{
		static FString TaskName = TEXT("RTMPSendPacketsTask");
		return TaskName;
	}

	FRTMPStreamer::FRTMPStreamer(const FString& StreamerId)
		: StreamerId(StreamerId)
		, bIsStreaming(false)
		, VideoCapturer(FRTMPVideoCapturer::Create())
		, VideoSourceGroup(UE::PixelStreaming2::FVideoSourceGroup::Create(VideoCapturer))
		, VideoSource(FRTMPVideoSource::Create(VideoCapturer, VideoSourceGroup))
		, AudioCapturer(FAudioCapturer::Create<FRTMPAudioCapturer>())
	{
		Stream = MakeShared<FRTMP>(RTMP_Alloc());
		if (Stream->RtmpPtr == nullptr)
		{
			UE_LOGFMT(LogPixelStreamingRTMP, Log, "Failed to allocate RTMP object");
			return;
		}

		SendPacketsTask = UE::PixelStreaming2::FPixelStreamingTickableTask::Create<FSendPacketsTask>(Stream);

		RTMP_Init(Stream->RtmpPtr);

		// RTMP (in its original spec) only carries H.264, so we always encode H.264 rather than
		// honoring the Pixel Streaming encoder codec CVar.
		VideoEncoder = CreateVideoEncoder(EVideoCodec::H264);

		AudioEncoder = CreateAudioEncoder();
	}

	FRTMPStreamer::~FRTMPStreamer()
	{
		StopStreaming();

		if (Stream->RtmpPtr == nullptr)
		{
			return;
		}

		RTMP_Close(Stream->RtmpPtr);

		RTMP_Free(Stream->RtmpPtr);
	}

	void FRTMPStreamer::Initialize()
	{
		TSharedPtr<FRTMPStreamer> Streamer = AsShared();
		VideoSource->OnFramePushed.AddSP(Streamer.ToSharedRef(), &FRTMPStreamer::OnFramePushed);
		VideoEncoder->OnEncodedVideo.AddSP(Streamer.ToSharedRef(), &FRTMPStreamer::OnEncodedVideo);

		AudioCapturer->OnAudioBuffer.AddSP(Streamer.ToSharedRef(), &FRTMPStreamer::OnAudioBuffer);
		AudioEncoder->OnEncodedAudio.AddSP(Streamer.ToSharedRef(), &FRTMPStreamer::OnEncodedAudio);
	}

	void FRTMPStreamer::SetStreamFPS(int32 InFramesPerSecond)
	{
		VideoSourceGroup->SetFPS(InFramesPerSecond);
	}

	int32 FRTMPStreamer::GetStreamFPS()
	{
		return VideoSourceGroup->GetFPS();
	}

	void FRTMPStreamer::SetCoupleFramerate(bool bCouple)
	{
		VideoSourceGroup->SetDecoupleFramerate(!bCouple);
	}

	void FRTMPStreamer::SetVideoProducer(TSharedPtr<IPixelStreaming2VideoProducer> Producer)
	{
		VideoCapturer->SetVideoProducer(Producer);
	}

	TWeakPtr<IPixelStreaming2VideoProducer> FRTMPStreamer::GetVideoProducer()
	{
		return VideoCapturer->GetVideoProducer();
	}

    void FRTMPStreamer::AddAudioProducer(TSharedPtr<IPixelStreaming2AudioProducer> AudioProducer) 
	{
		{
			FScopeLock Lock(&CustomAudioProducersCS);
			CustomAudioProducers.Add(AudioProducer);
		}

		if (!AudioCapturer)
		{
			return;
		}

		AudioCapturer->AddAudioProducer(AudioProducer);
	}

	void FRTMPStreamer::RemoveAudioProducer(TSharedPtr<IPixelStreaming2AudioProducer> AudioProducer) 
	{
		{
			FScopeLock Lock(&CustomAudioProducersCS);
			if (CustomAudioProducers.Contains(AudioProducer))
			{
				CustomAudioProducers.Remove(AudioProducer);
			}
		}

		if (!AudioCapturer)
		{
			return;
		}

		AudioCapturer->RemoveAudioProducer(AudioProducer);
	}

	TArray<TWeakPtr<IPixelStreaming2AudioProducer>> FRTMPStreamer::GetAudioProducers() 
	{
		TArray<TWeakPtr<IPixelStreaming2AudioProducer>> WeakArray;
		WeakArray.SetNum(CustomAudioProducers.Num());

		Algo::Transform(CustomAudioProducers, WeakArray, [](const TSharedPtr<IPixelStreaming2AudioProducer>& Shared)
		{
    		return TWeakPtr<IPixelStreaming2AudioProducer>(Shared);
		});

		return WeakArray;
	}

	void FRTMPStreamer::SetConnectionURL(const FString& InConnectionURL)
	{
		ConnectionURL = InConnectionURL;
	}

	FString FRTMPStreamer::GetConnectionURL()
	{
		return ConnectionURL;
	}

	void FRTMPStreamer::SetStreamKey(const FString& InStreamKey)
	{
		StreamKey = InStreamKey;
	}

	FString FRTMPStreamer::GetId()
	{
		return StreamerId;
	}

	bool FRTMPStreamer::IsConnected()
	{
		return false;
	}

	void FRTMPStreamer::StartStreaming()
	{
		// Fresh stream: clear the shared timestamp base and re-arm the sequence-header guards so
		// this start re-sends onMetaData and the AAC sequence header (see the guard members).
		DtsOffset.Reset();
		bSentMetadata = false;
		bSentAudioHeader = false;

		// The base connection URL (from CVarConnectionURL) is the RTMP server/app; append the
		// stream key to form the full publish URL, e.g. rtmp://host:1935/app + "/" + key.
		FString FullURL = ConnectionURL;
		if (!StreamKey.IsEmpty())
		{
			if (!FullURL.EndsWith(TEXT("/")))
			{
				FullURL += TEXT("/");
			}
			FullURL += StreamKey;
		}

		char* RawURL = TCHAR_TO_ANSI(*FullURL);
		RTMP_SetupURL(Stream->RtmpPtr, RawURL);

		RTMP_EnableWrite(Stream->RtmpPtr);

		// connect to server
		if (!RTMP_Connect(Stream->RtmpPtr, NULL))
		{
			UE_LOGFMT(LogPixelStreamingRTMP, Warning, "Unable to connect to RTMP server");
			return;
		}

		if (!RTMP_ConnectStream(Stream->RtmpPtr, 0))
		{
			UE_LOGFMT(LogPixelStreamingRTMP, Warning, "Unable to connect to RTMP stream");
			return;
		}

		VideoSourceGroup->Start();

		bIsStreaming = true;
	}

	void FRTMPStreamer::StopStreaming()
	{
		VideoSourceGroup->Stop();

		if (Stream->RtmpPtr != nullptr)
		{
			RTMP_DeleteStream(Stream->RtmpPtr);
		}

		bIsStreaming = false;
	}

	bool FRTMPStreamer::IsStreaming() const
	{
		return bIsStreaming;
	}

	FRTMPStreamer::FPreConnectionEvent& FRTMPStreamer::OnPreConnection()
	{
		return StreamingPreConnectionEvent;
	}

	FRTMPStreamer::FStreamingStartedEvent& FRTMPStreamer::OnStreamingStarted()
	{
		return StreamingStartedEvent;
	}

	FRTMPStreamer::FStreamingStoppedEvent& FRTMPStreamer::OnStreamingStopped()
	{
		return StreamingStoppedEvent;
	}

	void FRTMPStreamer::ForceKeyFrame()
	{
		VideoSourceGroup->ForceKeyFrame();
	}

	void FRTMPStreamer::FreezeStream(UTexture2D* Texture)
	{
	}

	void FRTMPStreamer::UnfreezeStream()
	{
	}

	void FRTMPStreamer::SendAllPlayersMessage(FString MessageType, const FString& Descriptor)
	{
	}

	void FRTMPStreamer::SendPlayerMessage(FString PlayerId, FString MessageType, const FString& Descriptor)
	{
	}

	void FRTMPStreamer::SendFileData(const TArray64<uint8>& ByteData, FString& MimeType, FString& FileExtension)
	{
	}

	void FRTMPStreamer::KickPlayer(FString PlayerId)
	{
	}

	TArray<FString> FRTMPStreamer::GetConnectedPlayers()
	{
		return {};
	}

	TWeakPtr<IPixelStreaming2InputHandler> FRTMPStreamer::GetInputHandler()
	{

		return nullptr;
	}

	void FRTMPStreamer::SetInputHandler(TSharedPtr<IPixelStreaming2InputHandler> Handler)
	{
	}

	TWeakPtr<IPixelStreaming2AudioSink> FRTMPStreamer::GetPeerAudioSink(FString PlayerId)
	{

		return nullptr;
	}

	TWeakPtr<IPixelStreaming2AudioSink> FRTMPStreamer::GetUnlistenedAudioSink()
	{

		return nullptr;
	}

	TWeakPtr<IPixelStreaming2VideoSink> FRTMPStreamer::GetPeerVideoSink(FString PlayerId)
	{
		return nullptr;
	}

	TWeakPtr<IPixelStreaming2VideoSink> FRTMPStreamer::GetUnwatchedVideoSink()
	{
		return nullptr;
	}

	void FRTMPStreamer::SetConfigOption(const FName& OptionName, const FString& Value)
	{
	}

	bool FRTMPStreamer::GetConfigOption(const FName& OptionName, FString& OutValue)
	{
		return false;
	}

	void FRTMPStreamer::PlayerRequestsBitrate(FString PlayerId, int MinBitrate, int MaxBitrate)
	{
	}

	void FRTMPStreamer::RefreshStreamBitrate()
	{
	}

	void FRTMPStreamer::OnFramePushed(TSharedPtr<FRTMPVideoBufferMultiFormat> PushedFrame)
	{
		VideoEncoder->Encode(PushedFrame);
	}

	void FRTMPStreamer::OnEncodedVideo(FVideoPacket& Packet, int32 Width, int32 Height)
	{
		if (!DtsOffset.IsSet())
		{
			DtsOffset = Packet.Timestamp;
		}

		// Send the stream metadata (onMetaData) once per stream, mirroring how the audio/video
		// sequence headers are sent: build the AMF payload and enqueue it as a data packet.
		if (!bSentMetadata)
		{
			bSentMetadata = true;

			char  Metadata[4096];
			char* enc = Metadata;
			char* end = enc + sizeof(Metadata);

			EncodeString(&enc, end, "@setDataFrame");
			EncodeString(&enc, end, "onMetaData");

			*enc++ = AMF_ECMA_ARRAY;
			EncodeInt32(&enc, end, 7);

			EncodeNumber(&enc, end, "duration", 0.0);
			EncodeNumber(&enc, end, "fileSize", 0.0);
			EncodeNumber(&enc, end, "width", static_cast<double>(Width));
			EncodeNumber(&enc, end, "height", static_cast<double>(Height));
			EncodeNumber(&enc, end, "videocodecid", static_cast<double>(7));
			EncodeNumber(&enc, end, "videodatarate", static_cast<double>(2500));
			EncodeNumber(&enc, end, "framerate", static_cast<double>(60));

			*enc++ = 0;
			*enc++ = 0;
			*enc++ = AMF_OBJECT_END;

			EnqueuePacket(Stream, MakeDataPacket(reinterpret_cast<const uint8*>(Metadata), static_cast<uint32>(enc - Metadata)));
		}

		EVideoCodec CurrentCodec = Packet.CodecSpecificInfo.Codec;
		switch (CurrentCodec)
		{
			case EVideoCodec::H264:
				HandleAvcPacket(Stream, Packet, Width, Height, DtsOffset.GetValue());
				break;
			case EVideoCodec::AV1:
			case EVideoCodec::VP8:
			case EVideoCodec::VP9:
			default:
				break;
		}
	}

	void FRTMPStreamer::OnAudioBuffer(const int16_t* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate)
	{
		AudioEncoder->Encode(AudioData, NumSamples, NumChannels, SampleRate);
	}

	void FRTMPStreamer::OnEncodedAudio(FAudioPacket& Packet, int32 NumChannels, const int32 SampleRate)
	{
		if (!DtsOffset.IsSet())
		{
			DtsOffset = Packet.Timestamp;
		}

		// Send the AAC sequence header (AudioSpecificConfig) once per stream before any raw
		// AAC frames, mirroring how the AVC SPS/PPS sequence header is sent for video.
		// Strict RTMP servers (e.g. MediaMTX) need it to set up the audio track, otherwise
		// they reject the stream with "received a packet for audio track 0, but track is not set".
		if (!bSentAudioHeader)
		{
			bSentAudioHeader = true;

			const uint8 ObjectType = 2; // AAC-LC
			// Map the sample rate to its MPEG-4 sampling frequency index.
			const uint8 RateIdx = SampleRateToMpeg4Index.FindRef(SampleRate);
			const uint8 ChannelConfig = static_cast<uint8>(NumChannels);

			// 16-bit AudioSpecificConfig: 5 bits object type, 4 bits sample rate index,
			// 4 bits channel config, 3 bits trailing zero (GASpecificConfig).
			uint8 Asc[2];
			Asc[0] = (ObjectType << 3) | (RateIdx >> 1);
			Asc[1] = ((RateIdx & 0x1) << 7) | (ChannelConfig << 3);

			EnqueuePacket(Stream, MakeAudioPacket(Asc, 2, static_cast<uint32>(Packet.Timestamp - DtsOffset.GetValue()), true /* bIsHeader */));
		}

		HandleAacPacket(Stream, Packet, NumChannels, SampleRate, DtsOffset.GetValue());
	}

	FRTMPStreamerFactory::FRTMPStreamerFactory()
	{
		IPixelStreaming2StreamerFactory::RegisterStreamerFactory(this);
	}

	FRTMPStreamerFactory::~FRTMPStreamerFactory()
	{
		IPixelStreaming2StreamerFactory::UnregisterStreamerFactory(this);
	}

	FString FRTMPStreamerFactory::GetStreamType()
	{
		return RTMPStreamType;
	}

	TSharedPtr<IPixelStreaming2Streamer> FRTMPStreamerFactory::CreateNewStreamer(const FString& StreamerId)
	{
		TSharedPtr<FRTMPStreamer> NewStreamer = MakeShared<FRTMPStreamer>(StreamerId);
		NewStreamer->SetVideoProducer(UE::PixelStreaming2::FVideoProducerBackBuffer::Create());

		// Only the default streamer picks up the configured default stream key. The base
		// connection URL is set separately by the PixelStreaming2 framework (from
		// CVarConnectionURL); the key is appended to it when we connect.
		if (StreamerId == UPixelStreaming2PluginSettings::CVarDefaultStreamerID.GetValueOnAnyThread())
		{
			NewStreamer->SetStreamKey(UPixelStreamingRTMPPluginSettings::CVarDefaultStreamKey.GetValueOnAnyThread());
		}

		return NewStreamer;
	}
} // namespace UE::PixelStreamingRTMP
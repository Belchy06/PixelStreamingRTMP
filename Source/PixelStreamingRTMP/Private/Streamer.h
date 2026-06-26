// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelStreaming2Streamer.h"
#include "RTMPAudioCapturer.h"
#include "RTMPAudioEncoder.h"
#include "RTMPVideoBufferMultiFormat.h"
#include "RTMPVideoCapturer.h"
#include "RTMPVideoEncoder.h"
#include "RTMPVideoSource.h"
#include "TickableTask.h"
#include "UtilsRTMP.h"
#include "VideoSource.h"
#include "VideoSourceGroup.h"

#include "rtmp.h"

namespace UE::PixelStreamingRTMP
{
	using namespace UE::PixelStreaming2;

	class FSendPacketsTask : public FPixelStreamingTickableTask
	{
	public:
		FSendPacketsTask(TSharedPtr<FRTMP> Stream);
		virtual ~FSendPacketsTask() = default;

		// Begin FPixelStreamingTickableTask
		virtual void		   Tick(float DeltaMs) override;
		virtual const FString& GetName() const override;
		// End FPixelStreamingTickableTask

	private:
		TSharedPtr<FRTMP> Stream;
	};

	class FRTMPStreamer : public IPixelStreaming2Streamer, public TSharedFromThis<FRTMPStreamer>
	{
	public:
		FRTMPStreamer(const FString& StreamerId);
		virtual ~FRTMPStreamer() override;

		virtual void											Initialize() override;
		virtual void											SetStreamFPS(int32 InFramesPerSecond) override;
		virtual int32											GetStreamFPS() override;
		virtual void											SetCoupleFramerate(bool bCouple) override;
		virtual void											SetVideoProducer(TSharedPtr<IPixelStreaming2VideoProducer> Producer) override;
		virtual TWeakPtr<IPixelStreaming2VideoProducer>			GetVideoProducer() override;
		virtual void											AddAudioProducer(TSharedPtr<IPixelStreaming2AudioProducer> AudioProducer) override;
		virtual void											RemoveAudioProducer(TSharedPtr<IPixelStreaming2AudioProducer> AudioProducer) override;
		virtual TArray<TWeakPtr<IPixelStreaming2AudioProducer>> GetAudioProducers() override;
		virtual void											SetConnectionURL(const FString& InConnectionURL) override;
		virtual FString											GetConnectionURL() override;
		virtual FString											GetId() override;
		virtual bool											IsConnected() override;
		virtual void											StartStreaming() override;
		virtual void											StopStreaming() override;
		virtual bool											IsStreaming() const;
		virtual FPreConnectionEvent&							OnPreConnection() override;
		virtual FStreamingStartedEvent&							OnStreamingStarted() override;
		virtual FStreamingStoppedEvent&							OnStreamingStopped() override;
		virtual void											ForceKeyFrame() override;
		virtual void											FreezeStream(UTexture2D* Texture) override;
		virtual void											UnfreezeStream() override;
		virtual void											SendAllPlayersMessage(FString MessageType, const FString& Descriptor) override;
		virtual void											SendPlayerMessage(FString PlayerId, FString MessageType, const FString& Descriptor) override;
		virtual void											SendFileData(const TArray64<uint8>& ByteData, FString& MimeType, FString& FileExtension) override;
		virtual void											KickPlayer(FString PlayerId) override;
		virtual TArray<FString>									GetConnectedPlayers() override;
		virtual TWeakPtr<IPixelStreaming2InputHandler>			GetInputHandler() override;
		virtual void											SetInputHandler(TSharedPtr<IPixelStreaming2InputHandler> Handler) override;
		virtual TWeakPtr<IPixelStreaming2AudioSink>				GetPeerAudioSink(FString PlayerId) override;
		virtual TWeakPtr<IPixelStreaming2AudioSink>				GetUnlistenedAudioSink() override;
		virtual TWeakPtr<IPixelStreaming2VideoSink>				GetPeerVideoSink(FString PlayerId) override;
		virtual TWeakPtr<IPixelStreaming2VideoSink>				GetUnwatchedVideoSink() override;
		virtual void											SetConfigOption(const FName& OptionName, const FString& Value) override;
		virtual bool											GetConfigOption(const FName& OptionName, FString& OutValue) override;
		virtual void											PlayerRequestsBitrate(FString PlayerId, int MinBitrate, int MaxBitrate) override;
		virtual void											RefreshStreamBitrate() override;

	private:
		void OnFramePushed(TSharedPtr<FRTMPVideoBufferMultiFormat> PushedFrame);
		void OnEncodedVideo(FVideoPacket& Packet, int32 Width, int32 Height);
		void OnAudioBuffer(const int16_t* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate);
		void OnEncodedAudio(FAudioPacket& Packet, int32 NumChannels, const int32 SampleRate);

	private:
		TSharedPtr<FRTMP> Stream;

		FPreConnectionEvent	   StreamingPreConnectionEvent;
		FStreamingStartedEvent StreamingStartedEvent;
		FStreamingStoppedEvent StreamingStoppedEvent;

		FString			  StreamerId;
		FString			  ConnectionURL;
		bool			  bIsStreaming;
		TOptional<uint64> DtsOffset;

		TSharedPtr<FRTMPVideoCapturer> VideoCapturer;
		TSharedPtr<FVideoSourceGroup>  VideoSourceGroup;
		TSharedPtr<FRTMPVideoSource>   VideoSource;
		TSharedPtr<IRTMPVideoEncoder>  VideoEncoder;

		TSharedPtr<FRTMPAudioCapturer> AudioCapturer;
		TSharedPtr<IRTMPAudioEncoder>  AudioEncoder;

		TSharedPtr<FSendPacketsTask> SendPacketsTask;

		FCriticalSection								  CustomAudioProducersCS;
		TArray<TSharedPtr<IPixelStreaming2AudioProducer>> CustomAudioProducers;
	};

	class FRTMPStreamerFactory : public IPixelStreaming2StreamerFactory
	{
	public:
		FRTMPStreamerFactory();
		virtual ~FRTMPStreamerFactory() override;

		virtual FString								 GetStreamType() override;
		virtual TSharedPtr<IPixelStreaming2Streamer> CreateNewStreamer(const FString& StreamerId) override;
	};
} // namespace UE::PixelStreamingRTMP
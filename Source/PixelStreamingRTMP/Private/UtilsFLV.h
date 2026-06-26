#pragma once

#include "Logging.h"

#include "Serialization.h"
#include "Serialization/BufferArchive.h"
#include "UtilsRTMP.h"

#include "rtmp.h"

namespace UE::PixelStreamingRTMP
{
	// Legacy FLV videocodecid. Only H.264 is expressible in legacy RTMP; VP8/VP9/AV1
	// require Enhanced RTMP (FourCC) which this plugin does not yet mux.
	inline static const TMap<EVideoCodec, int32> VideoCodecToFlvId = {
    	{ EVideoCodec::H264, 7 },
	};
	
	// Copies an assembled message body into a packet of the requested type.
	template <typename TPacket>
	inline TPacket MakeRTMPPacket(const FBufferArchive64& Body, uint32 Timestamp)
	{
		TPacket Packet;
		Packet.Body = MakeShareable(new uint8[Body.Num()]);
		FMemory::BigBlockMemcpy(Packet.Body.Get(), Body.GetData(), Body.Num());
		Packet.BodySize = static_cast<uint32>(Body.Num());
		Packet.Timestamp = Timestamp;
		return Packet;
	}

	// AAC audio message body: [SoundFormat byte][AACPacketType][payload].
	inline FRTMPAudioPacket MakeAudioPacket(const uint8* Payload, uint32 PayloadSize, uint32 Timestamp, bool bIsHeader)
	{
		FBufferArchive64 Body;

		// 0xaf = 10101111: SoundFormat=AAC, 44kHz, 16-bit, stereo (rate/size/type ignored for AAC).
		s_w8(Body, 0xaf);
		s_w8(Body, bIsHeader ? 0 : 1); // 0 = sequence header, 1 = raw AAC
		s_w(Body, Payload, PayloadSize);

		return MakeRTMPPacket<FRTMPAudioPacket>(Body, Timestamp);
	}

	// AVC video message body: [FrameType+CodecId][AVCPacketType][compositionTime:3][payload].
	inline FRTMPVideoPacket MakeVideoPacket(const uint8* Payload, uint32 PayloadSize, uint32 Timestamp, bool bIsKeyframe, bool bIsHeader)
	{
		FBufferArchive64 Body;

		s_w8(Body, bIsKeyframe ? 0x17 : 0x27); // FrameType 1=key/2=inter, CodecId 7 = AVC
		s_w8(Body, bIsHeader ? 0 : 1);		   // 0 = sequence header, 1 = NALU
		s_wb24(Body, 0);					   // composition time offset
		s_w(Body, Payload, PayloadSize);

		return MakeRTMPPacket<FRTMPVideoPacket>(Body, Timestamp);
	}

	// Script-data message body (e.g. onMetaData): the AMF payload as-is.
	inline FRTMPDataPacket MakeDataPacket(const uint8* Payload, uint32 PayloadSize)
	{
		FRTMPDataPacket Packet;
		Packet.Body = MakeShareable(new uint8[PayloadSize]);
		FMemory::BigBlockMemcpy(Packet.Body.Get(), Payload, PayloadSize);
		Packet.BodySize = PayloadSize;
		Packet.Timestamp = 0;
		return Packet;
	}
} // namespace UE::PixelStreamingRTMP

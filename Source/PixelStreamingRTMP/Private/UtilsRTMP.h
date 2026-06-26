#pragma once

#include "Containers/Queue.h"
#include "Misc/TVariant.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

#include "rtmp.h"

#include <type_traits>

namespace UE::PixelStreamingRTMP
{
	// Chunk stream IDs (channels). Audio and video are deliberately on separate channels:
	// librtmp keeps a per-channel "last timestamp" and computes relative deltas against it.
	// Interleaving two media types on one channel yields negative deltas (their timestamps
	// don't advance in lockstep), which librtmp encodes as corrupt extended timestamps that
	// strict servers reject. Separate channels keep each timeline monotonic.
	inline constexpr int RTMP_CHANNEL_DATA = 0x05;
	inline constexpr int RTMP_CHANNEL_VIDEO = 0x06;
	inline constexpr int RTMP_CHANNEL_AUDIO = 0x07;

	// A muxed RTMP message ready for RTMP_SendPacket. Each concrete type carries the chunk
	// stream and RTMP packet type it belongs to, so the dequeue side dispatches on the type
	// instead of inspecting the bytes.
	struct FRTMPPacket
	{
		TSharedPtr<uint8> Body;			 // RTMP message payload (codec data, no FLV tag framing)
		uint32			  BodySize = 0;
		uint32			  Timestamp = 0; // milliseconds, relative to stream start
	};

	struct FRTMPAudioPacket : public FRTMPPacket
	{
		static constexpr uint8 RTMPType = RTMP_PACKET_TYPE_AUDIO;
		static constexpr int   RTMPChannel = RTMP_CHANNEL_AUDIO;
	};

	struct FRTMPVideoPacket : public FRTMPPacket
	{
		static constexpr uint8 RTMPType = RTMP_PACKET_TYPE_VIDEO;
		static constexpr int   RTMPChannel = RTMP_CHANNEL_VIDEO;
	};

	struct FRTMPDataPacket : public FRTMPPacket
	{
		static constexpr uint8 RTMPType = RTMP_PACKET_TYPE_INFO;
		static constexpr int   RTMPChannel = RTMP_CHANNEL_DATA;
	};

	using FRTMPPacketVariant = TVariant<FRTMPAudioPacket, FRTMPVideoPacket, FRTMPDataPacket>;

	class FRTMP
	{
	public:
		FRTMP(RTMP* RtmpPtr)
			: RtmpPtr(RtmpPtr)
		{
		}

	public:
		// Multiple producers (the audio and video encode threads both enqueue) feeding a
		// single consumer (FSendPacketsTask). The default Spsc mode is unsafe here.
		TQueue<FRTMPPacketVariant, EQueueMode::Mpsc> Packets;

		RTMP* RtmpPtr = nullptr;
	};

	// Wraps a concrete packet in the queue's variant and enqueues it. Safe to call from any
	// producer thread.
	template <typename TPacket>
	inline void EnqueuePacket(TSharedPtr<FRTMP> Stream, TPacket&& Packet)
	{
		using TConcrete = std::decay_t<TPacket>;
		Stream->Packets.Enqueue(FRTMPPacketVariant(TInPlaceType<TConcrete>(), Forward<TPacket>(Packet)));
	}

	static inline AVal* flv_str(AVal* out, const char* str)
	{
		out->av_val = (char*)str;
		out->av_len = (int)strlen(str);
		return out;
	}

	static inline void EncodeNumber(char** enc, char* end, const char* name,
		double val)
	{
		AVal s;
		*enc = AMF_EncodeNamedNumber(*enc, end, flv_str(&s, name), val);
	}

	static inline void EncodeInt32(char** enc, char* end, int32 val)
	{
		*enc = AMF_EncodeInt32(*enc, end, val);
	}

	static inline void EncodeBool(char** enc, char* end, const char* name,
		bool val)
	{
		AVal s;
		*enc = AMF_EncodeNamedBoolean(*enc, end, flv_str(&s, name), val);
	}

	static inline void EncodeStringValue(char** enc, char* end, const char* name,
		const char* val)
	{
		AVal s1, s2;
		*enc = AMF_EncodeNamedString(*enc, end, flv_str(&s1, name),
			flv_str(&s2, val));
	}

	static inline void EncodeString(char** enc, char* end, const char* str)
	{
		AVal s;
		*enc = AMF_EncodeString(*enc, end, flv_str(&s, str));
	}

} // namespace UE::PixelStreamingRTMP
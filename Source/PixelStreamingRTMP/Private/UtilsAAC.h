#pragma once

#include "Logging.h"

#include "Serialization.h"
#include "Serialization/BufferArchive.h"
#include "UtilsFLV.h"
#include "UtilsRTMP.h"

#include "rtmp.h"

namespace UE::PixelStreamingRTMP
{
	inline static const TMap<int32, uint8> SampleRateToMpeg4Index = {
		{96000, 0}, 
		{88200, 1}, 
		{64000, 2}, 
		{48000, 3}, 
		{44100, 4}, 
		{32000, 5},
		{24000, 6}, 
		{22050, 7}, 
		{16000, 8}, 
		{12000, 9}, 
		{11025, 10}, 
		{8000, 11},
		{7350, 12}
	};

	inline void HandleAacPacket(TSharedPtr<FRTMP> Stream, FAudioPacket& Packet, int32 NumChannels, const int32 SampleRate, uint64 DtsOffset)
	{
		EnqueuePacket(Stream, MakeAudioPacket(Packet.DataPtr.Get(), Packet.DataSize, static_cast<uint32>(Packet.Timestamp - DtsOffset), false /* bIsHeader */));
	}
} // namespace UE::PixelStreamingRTMP
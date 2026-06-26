#pragma once

#include "Logging.h"

#include "Serialization.h"
#include "Serialization/BufferArchive.h"
#include "Video/Decoders/Configs/VideoDecoderConfigH264.h"
#include "UtilsFLV.h"
#include "UtilsRTMP.h"
#include "UtilsVideo.h"

#include "rtmp.h"

namespace UE::PixelStreamingRTMP
{
	using namespace UE::AVCodecCore::H264;
	static FVideoDecoderConfigH264 H264;

	inline void HandleAvcPacket(TSharedPtr<FRTMP> Stream, FVideoPacket& Packet, int32 Width, int32 Height, uint64 DtsOffset)
	{
		TArray<FNaluH264> FoundNalus;

		FAVResult Result = FindNALUs(Packet, FoundNalus);
		if (Result.IsNotSuccess())
		{
			return;
		}

		const uint32 Timestamp = static_cast<uint32>(Packet.Timestamp - DtsOffset);

		for (auto& Nal : FoundNalus)
		{
			switch (Nal.Type)
			{
				case ENaluType::SliceOfNonIdrPicture:
				case ENaluType::SliceIdrPicture:
				{
					const bool bIsKeyframe = Nal.Type == ENaluType::SliceIdrPicture;

					// Prepend the NAL header byte and a 4-byte length prefix (AVCC format).
					Nal.Data--;

					FBufferArchive64 NalBuffer;
					s_wb32(NalBuffer, (uint32)Nal.Size);
					s_w(NalBuffer, Nal.Data, Nal.Size);

					EnqueuePacket(Stream, MakeVideoPacket(NalBuffer.GetData(), (uint32)NalBuffer.Num(), Timestamp, bIsKeyframe, false /* bIsHeader */));
					break;
				}
				case ENaluType::SequenceParameterSet:
				{
					// Combine SPS and PPS into a single AVCDecoderConfigurationRecord
					FNaluH264* SPS = &Nal;
					FNaluH264* PPS = nullptr;
					for (auto& OtherNal : FoundNalus)
					{
						if (OtherNal.Type == ENaluType::PictureParameterSet)
						{
							PPS = &OtherNal;
							break;
						}
					}

					if (!PPS)
					{
						break;
					}

					// We need to include the first byte in the packet
					// as it contains the type, refidc and zero bit
					SPS->Data--;
					PPS->Data--;

					FBufferArchive64 HeaderBuffer;
					s_w8(HeaderBuffer, 0x01);

					s_w8(HeaderBuffer, (uint8)(SPS->Data + 1)[0]);
					s_w8(HeaderBuffer, (uint8)(SPS->Data + 2)[0]);
					s_w8(HeaderBuffer, (uint8)(SPS->Data + 3)[0]);

					s_w8(HeaderBuffer, 0xff);
					s_w8(HeaderBuffer, 0xe1);

					s_wb16(HeaderBuffer, (uint16)SPS->Size);
					s_w(HeaderBuffer, SPS->Data, SPS->Size);
					s_w8(HeaderBuffer, 0x01);
					s_wb16(HeaderBuffer, (uint16)PPS->Size);
					s_w(HeaderBuffer, PPS->Data, PPS->Size);

					EnqueuePacket(Stream, MakeVideoPacket(HeaderBuffer.GetData(), (uint32)HeaderBuffer.Num(), Timestamp, false /* bIsKeyframe */, true /* bIsHeader */));
					break;
				}
				default:
					continue;
			}
		}
	}
} // namespace UE::PixelStreamingRTMP
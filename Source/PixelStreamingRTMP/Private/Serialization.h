#pragma once

#include "Logging.h"

#include "Serialization.h"
#include "Serialization/BufferArchive.h"
#include "Video/Decoders/Configs/VideoDecoderConfigH264.h"

namespace UE::PixelStreamingRTMP
{
	inline void s_w8(FBufferArchive64& Buffer, uint8 u8)
	{
		Buffer << u8;
	}

	inline void s_w(FBufferArchive64& Buffer, const uint8* Data, uint64 Size)
	{
		for (uint64 i = 0; i < Size; i++)
		{
			s_w8(Buffer, Data[i]);
		}
	}

	inline void s_wl16(FBufferArchive64& Buffer, uint16 u16)
	{
		s_w8(Buffer, (uint8)(u16));
		s_w8(Buffer, (u16 >> 8));
	}

	inline void s_wl24(FBufferArchive64& Buffer, uint32 u24)
	{
		s_w8(Buffer, (uint8)(u24));
		s_wl16(Buffer, (uint16)(u24 >> 8));
	}

	inline void s_wl32(FBufferArchive64& Buffer, uint32 u32)
	{
		s_wl16(Buffer, (uint16)(u32));
		s_wl16(Buffer, (uint16)(u32 >> 16));
	}

	inline void s_wl64(FBufferArchive64& Buffer, uint64 u64)
	{
		s_wl32(Buffer, (uint32)(u64));
		s_wl32(Buffer, (uint32)(u64 >> 32));
	}

	inline void s_wb16(FBufferArchive64& Buffer, uint16 u16)
	{
		s_w8(Buffer, (u16 >> 8));
		s_w8(Buffer, (uint8)(u16));
	}

	inline void s_wb24(FBufferArchive64& Buffer, uint32 u24)
	{
		s_wb16(Buffer, (uint16)(u24 >> 8));
		s_w8(Buffer, (uint8)(u24));
	}

	inline void s_wb32(FBufferArchive64& Buffer, uint32 u32)
	{
		s_wb16(Buffer, (uint16)(u32 >> 16));
		s_wb16(Buffer, (uint16)(u32));
	}

	inline void s_wb64(FBufferArchive64& Buffer, uint64 u64)
	{
		s_wb32(Buffer, (uint32)(u64 >> 32));
		s_wb32(Buffer, (uint32)(u64));
	}
} // namespace UE::PixelStreamingRTMP
#include "RTMPLogging.h"

namespace UE::PixelStreamingRTMP
{
	void RedirectRTMPLogsToUnreal(int Level, const char* Format, va_list Args)
	{
		static const ELogVerbosity::Type RTMPToUnrealLogCategoryMap[] = {
			ELogVerbosity::Error,
			ELogVerbosity::Error,
			ELogVerbosity::Warning,
			ELogVerbosity::Log,
			ELogVerbosity::Verbose,
			ELogVerbosity::VeryVerbose
		};

		if (LogPixelStreamingRTMP.IsSuppressed(RTMPToUnrealLogCategoryMap[Level]))
		{
			return;
		}

		char Str[2048] = "";
		vsnprintf(Str, 2048 - 1, Format, Args);

		FString Msg(Str);

		switch (Level)
		{
			case RTMP_LOGCRIT:
			case RTMP_LOGERROR:
			{
				UE_LOG(LogPixelStreamingRTMP, Error, TEXT("%s"), *Msg);
				break;
			}
			case RTMP_LOGWARNING:
			{
				UE_LOG(LogPixelStreamingRTMP, Warning, TEXT("%s"), *Msg);
				break;
			}
			case RTMP_LOGINFO:
			{
				UE_LOG(LogPixelStreamingRTMP, Log, TEXT("%s"), *Msg);
				break;
			}
			case RTMP_LOGDEBUG:
			{
				UE_LOG(LogPixelStreamingRTMP, Verbose, TEXT("%s"), *Msg);
				break;
			}
			case RTMP_LOGDEBUG2:
			{
				UE_LOG(LogPixelStreamingRTMP, VeryVerbose, TEXT("%s"), *Msg);
				break;
			}
		}
	}
} // namespace UE::PixelStreamingRTMP
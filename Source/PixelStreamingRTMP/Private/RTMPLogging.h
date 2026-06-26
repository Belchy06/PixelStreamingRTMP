#pragma once

#include "HAL/IConsoleManager.h"
#include "Logging.h"
#include "Logging/LogMacros.h"

#include "log.h"

namespace UE::PixelStreamingRTMP
{
	void RedirectRTMPLogsToUnreal(int Level, const char* Format, va_list Args);
} // namespace UE::PixelStreamingRTMP
#include "UtilsCore.h"
#include "Logging.h"
#include "RTMPLogging.h"
#include "Streamer.h"

namespace UE::PixelStreamingRTMP
{
    class FPixelStreamingRTMPModule : public IModuleInterface
    {
    public:
        // Begin IModuleInterface
        virtual void StartupModule() override
        {
            if (!UE::PixelStreaming2::IsStreamingSupported())
            {
                UE_LOGFMT(LogPixelStreamingRTMP, Error, "Pixel Streaming RTMP plugin is not supported on this platform.");
                return;
            }

            RTMP_LogSetLevel(RTMP_LOGALL);

		    RTMP_LogSetCallback(RedirectRTMPLogsToUnreal);
        }
        
        virtual void ShutdownModule() override
        {
            if (!UE::PixelStreaming2::IsStreamingSupported())
            {
                UE_LOGFMT(LogPixelStreamingRTMP, Error, "Pixel Streaming RTMP plugin is not supported on this platform.");
                return;
            }
        }
        // End IModuleInterface
        
    private:
        FRTMPStreamerFactory StreamerFactory;
    };
}

IMPLEMENT_MODULE(UE::PixelStreamingRTMP::FPixelStreamingRTMPModule, PixelStreamingRTMP);

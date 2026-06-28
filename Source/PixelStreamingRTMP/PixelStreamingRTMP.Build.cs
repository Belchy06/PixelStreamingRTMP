using System;
using System.IO;
using System.Collections.Generic;
using EpicGames.Core;

namespace UnrealBuildTool.Rules
{
	public class PixelStreamingRTMP : ModuleRules
	{
		public PixelStreamingRTMP(ReadOnlyTargetRules Target) : base(Target)
		{
			// This is so for game projects using our public headers don't have to include extra modules they might not know about.
			PublicDependencyModuleNames.AddRange(new string[] {});

			// NOTE: General rule is not to access the private folder of another module
			PrivateIncludePaths.AddRange(new string[]
			{
				System.IO.Path.Combine(GetModuleDirectory("PixelStreaming2"), "Internal"),
				System.IO.Path.Combine(GetModuleDirectory("PixelStreaming2Settings"), "Internal"),
				System.IO.Path.Combine(GetModuleDirectory("PixelStreamingRTMPSettings"), "Internal"),
			});

			PrivateDependencyModuleNames.AddRange(new string[]
			{
                "Core",
				"CoreUObject",
				"Engine",
				"librtmp",
				"PixelStreaming2",
				"PixelStreaming2Core",
				"PixelStreaming2Settings",
				"PixelStreamingRTMPSettings",
				"AVCodecsCore",
                "AVCodecsCoreRHI",
				"PixelCapture",
				"RHI",
				"SignalProcessing"
			});
		}
	}
}
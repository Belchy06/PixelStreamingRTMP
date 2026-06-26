using UnrealBuildTool;
using System.Collections.Generic;
using System.IO;

namespace UnrealBuildTool.Rules
{

    public class librtmp : ModuleRules
    {
        public bool bIsLinkedDynamic
        {
            get => false;
        }

        protected string ConfigPath { get; private set; }

        protected string GetPlatformName(UnrealTargetPlatform Platform)
        {
            if (Platform == UnrealTargetPlatform.Win64)
            {
                return "Windows";
            }
            else if (Platform == UnrealTargetPlatform.Mac)
            {
                return "Mac";
            }
            else if (Platform == UnrealTargetPlatform.Linux || Platform == UnrealTargetPlatform.LinuxArm64)
            {
                return "Linux";
            }

            return "";
        }

        protected string GetPlatformArchitecture(UnrealTargetPlatform Platform)
        {
            if (Platform == UnrealTargetPlatform.Win64 || Platform == UnrealTargetPlatform.Linux)
            {
                return "x86_64";
            }
            else if (Platform == UnrealTargetPlatform.Mac)
            {
                // Uses fatlib containing symbols for aarch64 and x86_64, so don't bother with an arch folder
                return "";
            }
            else if (Platform == UnrealTargetPlatform.LinuxArm64)
            {
                return "aarch64";
            }

            return "";
        }

        protected string GetPlatformExtension(UnrealTargetPlatform Platform)
        {
            if (Platform == UnrealTargetPlatform.Win64)
            {
                if (bIsLinkedDynamic)
                {
                    return ".dll";
                }
                else
                {
                    return ".lib";
                }
            }
            else if (Platform == UnrealTargetPlatform.Mac || Platform == UnrealTargetPlatform.Linux || Platform == UnrealTargetPlatform.LinuxArm64)
            {
                if (bIsLinkedDynamic)
                {
                    return ".so";
                }
                else
                {
                    return ".a";
                }
            }

            return "";
        }

        public librtmp(ReadOnlyTargetRules Target) : base(Target)
        {
            Type = ModuleType.External;

            if (Target.Configuration != UnrealTargetConfiguration.Debug)
            {
                ConfigPath = "Release";
            }
            else
            {
                // The debug webrtc binares are not portable, so we only ship with the release binaries
                // If you wanted, you would need to compile the webrtc binaries in debug and place the Lib and Include folder in the relevant location
                // ConfigPath = "Debug";
                ConfigPath = "Release";
            }

            PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "Include"));

            string PlatformName = GetPlatformName(Target.Platform);
            string PlatformArchitecture = GetPlatformArchitecture(Target.Platform);
            string PlatformFileExtension = GetPlatformExtension(Target.Platform);

            if (PlatformName == "")
            {
                //Logger.LogError("librtmp is not currently shipping with platform " + Target.Platform.ToString());
                return;
            }

            string LibraryName = "librtmp" + PlatformFileExtension;
            string LibrarySourcePath = Path.Combine(ModuleDirectory, "Lib", PlatformName, PlatformArchitecture, ConfigPath, LibraryName);

            if (bIsLinkedDynamic)
            {
                string RuntimeLibraryTargetPath = Path.Combine("$(TargetOutputDir)", LibraryName);

                PublicAdditionalLibraries.Add(LibrarySourcePath);

                RuntimeDependencies.Add(
                    RuntimeLibraryTargetPath,
                    LibrarySourcePath,
                    StagedFileType.NonUFS);

                PublicSystemLibraryPaths.Add(LibrarySourcePath);
                PublicRuntimeLibraryPaths.Add("$(TargetOutputDir)");
                PublicDelayLoadDLLs.Add(LibraryName);

                // TODO we likely need to load additional modules here
            }
            else
            {
                PublicAdditionalLibraries.Add(LibrarySourcePath);
            }

            if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				// Additional System library
				PublicSystemLibraries.Add("Secur32.lib");

				// The version of librtmp we depend on, depends on an openssl that depends on zlib
				AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
			}

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
        }
    }
}
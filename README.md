# Briefcase Suspicion Control

Suspicion Control is a native BriefcaseNative server mod that configures the suspicion system on a Deceive Inc. dedicated server. The repository retains its historical `Briefcase.StaminaControl` name for compatibility.

## Installation

1. Install [BriefcaseNative](https://github.com/EnoPM/BriefcaseNative) on the dedicated server.
2. Download the release archive that matches the server platform: `windows-x64` or `linux-x64`.
3. Stop the server and extract the archive into its binary directory (`Binaries/Win64` on Windows or `Binaries/Linux` on Linux).
4. Keep an existing `Briefcase/Mods/briefcase.suspicion-control/Data/config.json` file when updating.
5. Start the server through the Briefcase launcher.

BriefcaseNative 0.6.0 and later can update an installed copy automatically before the server starts. The mod repository must be publicly accessible for anonymous update checks.

## Configuration

The configuration is stored in `Briefcase/Mods/briefcase.suspicion-control/Data/config.json`. It can also be edited through the Briefcase server administration interface.

## Build and test

Windows builds require the Visual Studio C++ x64 tools and PowerShell 7:

```powershell
./scripts/Build.ps1
```

The script downloads the pinned BriefcaseNative SDK, compiles and tests the real mod DLL against a mock ABI host, and creates the release archive. To use an extracted SDK without downloading it:

```powershell
./scripts/Build.ps1 -SdkPath C:/path/to/BriefcaseNative-SDK
```

Linux builds require x64 Linux, Clang 19, CMake 3.28, Ninja, and Python 3:

```sh
python3 scripts/build-linux.py --sdk /path/to/BriefcaseNative-SDK
```

The Linux package contains native code and data and does not require Python at runtime.

## Releases

`VERSION` is the only source of the mod version. A push to `main` that changes this file builds and tests Windows and Linux, then publishes both native archives. The workflow can also be started manually and can create a draft release.

GitHub records the SHA-256 digest of each uploaded asset. Existing release versions are never replaced.

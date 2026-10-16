# Contributing to Briefcase Suspicion Control

Player-facing installation instructions belong in `README.md`. This document covers development and release work.

## Windows build

Install the Visual Studio C++ x64 tools and PowerShell 7, then run:

```powershell
./scripts/Build.ps1
```

The script downloads the pinned BriefcaseNative SDK, builds the real mod DLL, runs the native tests against a mock ABI host, and creates the release archive. To use an extracted compatible server SDK:

```powershell
./scripts/Build.ps1 -SdkPath C:/path/to/BriefcaseNative-SDK
```

## Linux build

On x64 Linux, install Clang 19, CMake 3.28 or later, Ninja, and Python 3. Then run:

```bash
sdk=$(python3 scripts/fetch-sdk.py)
python3 scripts/build-linux.py --sdk "$sdk"
```

The resulting package is fully native and does not require Python at runtime.

## Releases

`VERSION` is the only source of the mod version. A push to `main` that changes `VERSION` publishes the tested Windows archive and builds the Linux archive. After the Windows release exists, run the Linux package workflow with `publish=true` from the same commit to attach the verified Linux archive and platform catalog.

GitHub records the SHA-256 digest of every uploaded asset. Existing release versions must never be replaced.

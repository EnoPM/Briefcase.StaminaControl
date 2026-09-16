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

`VERSION` is the only source of the mod version. A push to `main` that changes `VERSION` builds and tests Windows and Linux, then publishes both archives. The workflow can also be started manually and may keep the release as a draft.

GitHub records the SHA-256 digest of every uploaded asset. Existing release versions must never be replaced.

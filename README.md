# Briefcase Suspicion Control

Suspicion Control adjusts the stamina drain associated with suspicion on a Windows x64 Deceive Inc. dedicated server. The repository retains the historical StaminaControl name.

## Install

1. Install the [latest BriefcaseNative Windows server release](https://github.com/EnoPM/BriefcaseNative/releases/latest) and stop the dedicated server.
2. Download `Briefcase.StaminaControl-windows-x64-<version>.zip` from this mod's latest release.
3. Extract the ZIP directly into the server's `DeceiveInc/Binaries/Win64` directory, beside `DeceiveIncServer-Win64-Shipping.exe`.
4. Open `ue4ss/Mods/mods.txt` and add this line if it is not already present:

```text
BriefcaseSuspicionControl : 1
```

5. Start `DeceiveIncServer-Win64-Shipping.exe` with Win64 as its working directory. The Briefcase `version.dll` loads this mod through UE4SS.

When upgrading from an older Briefcase mod package, copy your desired settings and remove the old `Briefcase/Mods/briefcase.suspicion-control` folder before starting the server, so both versions do not run together. Keep your existing `Data/config.json` when replacing this UE4SS mod.

## Configure

Edit `ue4ss/Mods/BriefcaseSuspicionControl/Data/config.json` while the server is stopped:

```json
{
  "multiplier": 0,
  "diagnostics": false,
  "maximumSamples": 120
}
```

`multiplier` accepts 0–10. Use `1` for vanilla drain or `0` to disable the drain controlled by this mod. Leave `diagnostics` at `false` for normal play; `maximumSamples` limits diagnostic samples when enabled. Restart after changes.

## Remove

Stop the server, remove `ue4ss/Mods/BriefcaseSuspicionControl`, and remove its line from `ue4ss/Mods/mods.txt`. Restart the server.

# Briefcase Suspicion Control

Suspicion Control adjusts the stamina drain associated with suspicion on a Windows x64 Deceive Inc. dedicated server. The repository retains the historical StaminaControl name.

## Install

In Briefcase App, add `https://github.com/EnoPM/Briefcase.StaminaControl/releases/latest/download/catalog.json` as a marketplace source in Settings, then open the server's Mods → Available Mods tab and install Suspicion Control while the server is stopped. The app enables the mod automatically.

For manual installation:

1. Install the [latest BriefcaseNative Windows server release](https://github.com/EnoPM/BriefcaseNative/releases/latest) and stop the dedicated server.
2. Download `Briefcase.StaminaControl-windows-x64-<version>.zip` from this mod's latest release.
3. Extract the ZIP directly into the server's `DeceiveInc/Binaries/Win64` directory, beside `DeceiveIncServer-Win64-Shipping.exe`.
4. Open `ue4ss/Mods/mods.txt` and add this line if it is not already present:

```text
briefcasesuspicioncontrol : 1
```

5. Start `DeceiveIncServer-Win64-Shipping.exe` with Win64 as its working directory. The Briefcase `version.dll` loads this mod through UE4SS.

When upgrading from an older Briefcase mod package, copy your desired settings and remove the old `Briefcase/Mods/briefcase.suspicion-control` folder before starting the server, so both versions do not run together. Keep your existing `Data/config.json` when replacing this UE4SS mod.

## Configure

Edit `ue4ss/Mods/briefcasesuspicioncontrol/Data/config.json` while the server is stopped:

```json
{
  "multiplier": 0,
  "diagnostics": false,
  "maximumSamples": 120
}
```

The Briefcase form uses the mod's `Data/config.schema.json` to present `multiplier` as a slider from 0 to 1 in steps of 0.05. Use `1` for vanilla drain or `0` to disable the drain controlled by this mod. The underlying configuration parser still accepts values up to 10 for existing raw JSON configurations. Leave `diagnostics` at `false` for normal play; `maximumSamples` limits diagnostic samples when enabled. Restart after changes.

## Remove

Stop the server, remove `ue4ss/Mods/briefcasesuspicioncontrol`, and remove its line from `ue4ss/Mods/mods.txt`. Restart the server.

## License

This mod is licensed under the [MIT License](LICENSE). Third-party components retain their own licenses.

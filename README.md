# Briefcase Suspicion Control

Suspicion Control adjusts the stamina and suspicion drain applied by a Deceive Inc. dedicated server. The repository keeps its historical `Briefcase.StaminaControl` name for compatibility. This is a native server mod for [BriefcaseNative](https://github.com/EnoPM/BriefcaseNative) and supports Windows x64 and Linux x64 servers.

## Complete server installation

### 1. Install the Deceive Inc. dedicated server

Install [SteamCMD](https://developer.valvesoftware.com/wiki/SteamCMD), then download the dedicated server anonymously. App `5007710` is the Deceive Inc. dedicated server.

On Windows:

```powershell
steamcmd.exe +force_install_dir "C:\DeceiveIncServer" +login anonymous +app_update 5007710 validate +quit
```

On Linux:

```bash
./steamcmd.sh +force_install_dir /opt/deceive-inc-server +login anonymous +app_update 5007710 validate +quit
```

The server binary directory used throughout this guide is:

- Windows: `C:\DeceiveIncServer\DeceiveInc\Binaries\Win64`
- Linux: `/opt/deceive-inc-server/DeceiveInc/Binaries/Linux`

### 2. Install BriefcaseNative

Stop the server and open the [latest BriefcaseNative release](https://github.com/EnoPM/BriefcaseNative/releases/latest).

On Windows, download `BriefcaseNative-Server-windows-x64-<version>.zip` and extract it directly into `DeceiveInc\Binaries\Win64`. Run `Briefcase.ServerLauncher.exe` from that directory. BriefcaseNative 0.7.1 or later creates `Briefcase\launch.json` automatically on first launch.

On Linux, install the native runtime dependencies. For Ubuntu 24.04:

```bash
sudo apt-get update
sudo apt-get install --no-install-recommends libcurl4t64 libarchive13t64 ca-certificates unzip
```

Download `BriefcaseNative-Server-linux-x64-<version>.zip`, extract it directly into `DeceiveInc/Binaries/Linux`, and make the launcher executable:

```bash
chmod +x Briefcase.ServerLauncher
```

### 3. Install Suspicion Control

Open the [latest Suspicion Control release](https://github.com/EnoPM/Briefcase.StaminaControl/releases/latest) and download the archive for the server operating system:

- `Briefcase.StaminaControl-windows-x64-<version>.zip`
- `Briefcase.StaminaControl-linux-x64-<version>.zip`

Stop the server and extract the archive directly into the same server binary directory used for BriefcaseNative. The resulting layout must include:

```text
DeceiveInc/
└── Binaries/
    └── Win64/ or Linux/
        ├── Briefcase.ServerLauncher[.exe]
        └── Briefcase/
            └── Mods/
                └── briefcase.suspicion-control/
                    ├── briefcase.mod.json
                    ├── Briefcase.SuspicionControl.dll or Briefcase.SuspicionControl.so
                    └── Data/
                        └── config.json
```

Do not extract the archive into a second `Win64`, `Linux`, or `Briefcase` directory. When updating manually, keep the existing `Data/config.json` file.

### 4. Configure suspicion behavior

Edit `Briefcase/Mods/briefcase.suspicion-control/Data/config.json`:

```json
{
  "multiplier": 1,
  "diagnostics": false,
  "maximumSamples": 120
}
```

| Setting | Allowed values | Description |
| --- | --- | --- |
| `multiplier` | `0` to `10` | Multiplies suspicion-related stamina drain. `0` disables running drain and discrete stamina losses; `1` preserves vanilla behavior. |
| `diagnostics` | `true` or `false` | Enables bounded diagnostic logging. Disabled by default. |
| `maximumSamples` | `1` to `1000` | Maximum number of diagnostic samples retained or logged. |

This mod controls stamina losses associated with the suspicion system; it does not modify a separate heat system. Restart the server after changing these values. The settings can also be changed from the Briefcase server administration interface.

### 5. Start and verify the server

Always start the server through the Briefcase launcher so framework and mod updates run before the game starts.

On Windows, run this from `DeceiveInc\Binaries\Win64`:

```powershell
.\Briefcase.ServerLauncher.exe
```

On Linux, run this from `DeceiveInc/Binaries/Linux`:

```bash
./Briefcase.ServerLauncher
```

Check `Briefcase/Logs/BriefcaseNative.log` for a successful load of `briefcase.suspicion-control`. Launcher and update details are written to `Briefcase/Logs/launcher.log` and `Briefcase/Updates/last-result.json`.

## Automatic updates

BriefcaseNative 0.6.0 or later checks this repository's stable releases before starting the server. Automatic updates work when:

- this repository is publicly accessible;
- the installed manifest contains the `github-releases` update information supplied by a current release;
- `Briefcase/updater.json` has `enabled` set to `true` and does not set `updateMods` to `false`;
- the server is started or restarted through `Briefcase.ServerLauncher`.

If the mod was installed before automatic update metadata was added, install the latest release manually once. Briefcase preserves `Data/config.json` during subsequent automatic updates. A network or validation failure keeps the installed version and lets the server start.

## Remove the mod

Stop the server, remove `Briefcase/Mods/briefcase.suspicion-control`, then start the server through the Briefcase launcher.

## Contributing

Build, test, and release information is kept in [CONTRIBUTING.md](CONTRIBUTING.md).

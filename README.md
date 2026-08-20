# Roundtable Report

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI](https://github.com/timlangner/Roundtable-Report/actions/workflows/ci.yml/badge.svg)](https://github.com/timlangner/Roundtable-Report/actions/workflows/ci.yml)

Roundtable Report is an Elden Ring overlay mod for PC. It tracks **one character in one save file**, shows a compact death counter while you play, and can post a Discord summary when the game closes.

It does not keep a lifetime total across every Tarnished. Switching characters or using a different save (`ER0000.sl2`, Seamless Co-op `.co2`, or a Mod Engine 3 save) is treated as a different run.

The mod is read-only. It does not write player stats or your save file.

**Offline or Seamless Co-op only.** The DLL is injected into `eldenring.exe`. Official online play with Easy Anti-Cheat enabled is a ban risk. Launch the same way you already launch other DLL mods (EAC off).

This is an unofficial fan project, not affiliated with FromSoftware or Bandai Namco. Elden Ring names, map art, and related assets remain their property.

## Requirements

- Steam Elden Ring (PC)
- A DLL loader: Elden Ring Mod Loader (`Game\mods`) or [Mod Engine 3](https://github.com/garyttierney/me3/releases)
- A Discord incoming webhook if you want run summaries in a channel (optional)

To build from source you also need Visual Studio 2022 with C++ desktop tools and CMake 3.24+.

## Install

Get `EldenRing_StatsShare.dll` from a [GitHub Release](https://github.com/timlangner/Roundtable-Report/releases) or build from source, then pick the loader you already use.

### Elden Ring Mod Loader (`Game\mods`)

Copy these into `steamapps\common\ELDEN RING\Game\mods` (same folder as other native DLLs such as UnlockTheFps):

- `natives/EldenRing_StatsShare.dll`
- `natives/EldenRing_StatsShare.toml`
- `natives/maps/` (Discord location images)

The `.toml` must sit next to the DLL. Paste your Discord webhook URL into that file if you want summaries. Launch Elden Ring the same way you launch your other mods.

### Mod Engine 3

1. Install Mod Engine 3 and confirm you can launch Elden Ring from a `.me3` profile.
2. Copy this repo’s `dist` folder (or merge its files into an existing profile directory):
   - `EldenRing_StatsShare.me3`
   - `natives/EldenRing_StatsShare.dll`
   - `natives/EldenRing_StatsShare.toml`
   - `natives/maps/`
3. Create a Discord incoming webhook for the channel you want and paste it into `natives/EldenRing_StatsShare.toml` (next to the DLL):

```toml
[discord]
webhook_url = "https://discord.com/api/webhooks/..."
mode = "edit"
```

Leave `webhook_url` empty to keep local tracking only. Do not commit a real webhook URL.

4. Double-click `EldenRing_StatsShare.me3`, or add this block to an existing profile:

```toml
[[natives]]
path = "natives/EldenRing_StatsShare.dll"
load_early = false
finalizer = "stats_share_shutdown"
```

`load_early` must stay `false` so the D3D12 overlay can hook after the renderer exists.

### Overlay and Discord options

Edit the `.toml` next to the DLL:

```toml
[discord]
webhook_url = ""
mode = "new"   # "new" posts every close; "edit" updates this run's last message

[overlay]
toggle_hotkey = "alt+o"
show_session_deaths = true
anchor = "left_middle"
```

`anchor` can be `left_middle`, `right_middle`, `top_right`, `top_left`, `bottom_right`, or `bottom_left`.

## In game

- Compact plaque: `DEATHS` stacked over the count, plus session `+N` only after a death this load
- **Alt+O** toggles the HUD (or whatever you set as `toggle_hotkey`)
- The HUD hides on the title screen
- Switching characters switches the counter to that slot

On quit, the mod posts (or edits) Discord embeds for the **last loaded character only**:

- **Game Profile** — name, last grace, deaths, session deaths and time, level, journey, IGT, runes, flasks, and a schematic map pinned at the last location. Shadow of the Erdtree regions are named, and Scadutree Blessing / Revered Ash appear when that character has DLC progress.
- **Last boss** — who they last died to (the named boss bar), best try as remaining HP%, and the weapons / talismans stored for that record.
- **Build & journey** — last killed, attributes, and bosses down this journey when those fields are present.

If the game crashes before Discord is reached, `pending.json` is posted on the next launch.

Local data lives in `%APPDATA%\EldenRing_StatsShare\` (`runs\`, `pending.json`, `mod.log`). The webhook URL is never written to the log.

## Build from source

```powershell
cmake -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The Release build copies `EldenRing_StatsShare.dll` and `assets/maps` into `dist/natives/`.

## After a game patch

Memory offsets live in `src/game/offsets.cpp`. GameDataMan is found with the Grand Archives AOB (`48 8B 05 ?? ?? ?? ?? 48 85 C0 74 05 48 8B 40 58 C3 C3`). If the HUD freezes at 0 after an update, that pattern or the offset table needs a refresh.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md). Vulnerability reports go through [SECURITY.md](SECURITY.md), not public issues.

## License

[MIT](LICENSE). Third-party components are listed in [THIRD_PARTY.md](THIRD_PARTY.md).

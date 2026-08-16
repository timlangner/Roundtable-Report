# Elden Ring Stats Share

A Mod Engine 3 native DLL that shows a **per-character death counter** on screen and posts a Discord summary when the game closes. Stats are bound to **one character slot in one save file**, never a lifetime total across every Tarnished.

## Requirements

- Steam Elden Ring (PC)
- A DLL loader: Elden Ring Mod Loader (`Game\mods`) or [Mod Engine 3](https://github.com/garyttierney/me3/releases)
- Visual Studio 2022 with C++ desktop tools (to build)
- CMake 3.24+

**Offline or Seamless Co-op only.** This DLL is injected into `eldenring.exe`. Official online play with Easy Anti-Cheat enabled is a ban risk. Launch the same way you already launch other DLL mods (EAC off).

The mod is read-only. It does not write player stats or your `.sl2` / `.co2` file.

## Build

```powershell
cmake -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The Release build copies `EldenRing_StatsShare.dll` into `dist/natives/`.

## Install

### Elden Ring Mod Loader (`Game\mods`)

If you already drop DLLs into `steamapps\common\ELDEN RING\Game\mods` (same place as UnlockTheFps), copy both files there:

- `dist/natives/EldenRing_StatsShare.dll`
- `dist/natives/EldenRing_StatsShare.toml`

The `.toml` must sit next to the DLL. Paste your Discord webhook URL into that file before launching. Launch Elden Ring the same way you launch your other mods.

### Mod Engine 3

1. Install Mod Engine 3 and confirm you can launch Elden Ring from a `.me3` profile.
2. Copy this repo's `dist` folder (or merge its files into an existing profile directory):
   - `EldenRing_StatsShare.me3`
   - `natives/EldenRing_StatsShare.dll`
   - `natives/EldenRing_StatsShare.toml`
3. Create a Discord incoming webhook for the channel you want and paste it into `EldenRing_StatsShare.toml` (next to the DLL):

```toml
[discord]
webhook_url = "https://discord.com/api/webhooks/..."
mode = "edit"
```

4. Double-click `EldenRing_StatsShare.me3`, or add the `[[natives]]` block to your existing profile:

```toml
[[natives]]
path = "natives/EldenRing_StatsShare.dll"
load_early = false
finalizer = "stats_share_shutdown"
```

`load_early` must stay `false` so the D3D12 overlay can hook after the renderer exists.

## In game

- Compact left-middle plaque: `DEATHS` stacked over the count, plus session `+N` only after a death this load
- **Alt+O** toggles the HUD
- The HUD hides on the title screen
- Switching characters switches the counter to that slot
- A different save (`ER0000.sl2`, Seamless `ER0000.co2`, or an ME3 `savefile`) is a different run

On quit, the mod posts (or edits) a Discord embed for the **last loaded character only**: name, deaths, session deaths, level, journey, current region, boss-fight status, IGT, runes, and a schematic map pinned at the character's last location. Shadow of the Erdtree regions are named, and Scadutree Blessing / Revered Ash appear when that character has DLC progress.

`mode = "edit"` updates that run's existing message. `mode = "new"` posts a fresh message every close.

If the game crashes before Discord is reached, `pending.json` is posted on the next launch.

Local data lives in `%APPDATA%\EldenRing_StatsShare\` (`runs\`, `pending.json`, `mod.log`). The webhook URL is never written to the log.

## After a game patch

Memory offsets live in `src/game/offsets.cpp`. GameDataMan is found with the Grand Archives AOB (`48 8B 05 ?? ?? ?? ?? 48 85 C0 74 05 48 8B 40 58 C3 C3`). If the HUD freezes at 0 after an update, that pattern or the offset table needs a refresh.

## Manual checks

- Load character A, die once: HUD increments
- Load character B in the same file: HUD switches; A is unchanged
- Quit: Discord embed is B only, with save filename and slot
- Relaunch A, quit: A's Discord message is updated; B's is left alone
- Seamless `.co2` or an ME3 alt save is treated as another run
- Title screen: no HUD and no Discord identity

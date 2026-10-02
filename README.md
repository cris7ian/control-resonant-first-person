# CONTROL Resonant — Exploration First Person

## Calibrated preview: 0.2.3

A Windows x64 exploration-camera mod using CRLoader 1.0.0, CRModMenu 1.7.0, and Steam Input's Xbox layout.
The user confirmed that the calibrated exploration view works well.

- Double-tap **RS/R3** or **K** outside combat to toggle the view.
- Combat clears the request. Double-tap again after combat; it never resumes automatically.
- Protected menus suspend the override. Focus loss clears the request.
- Change calibration in **Options → MODS → Exploration First Person (Prototype)**.

Defaults: **distance −6.3, height −0.2, side 0, fine forward −0.05**.
Updates preserve existing settings. The native defaults and menu reset values use the same calibration.

**This remains an offset preview, not a finished player-eye camera.**
The game's third-person wall retraction still shifts the view. Collision, visibility, camera ownership, and full combat coverage remain unvalidated.
FOV remains native. Toggle off or disable Enabled if the view clips or behaves incorrectly.
Set `camera_writes=0` in the installed safety INI and restart for diagnostic-only operation.

## Install

Close the game. From this repository:

```powershell
python scripts/install.py           # build, test, stage, dry run
python scripts/install.py --apply   # verified, backed-up installation
```

Requires Windows x64, Python 3.10+, CMake, Ninja, WinLibs GCC/G++, and hash-pinned local dependency ZIPs.
See [installation](docs/INSTALLATION.md) for archive locations, alternate game paths, compiler paths, and receipt-based rollback.
The helper preserves the executable and calibration settings. It never changes game archives, saves, or `steam_api64.dll`.
Keep BetterCamera disabled: both mods would own the same camera hook.

The repository install skill is `.pi/skills/install-control-resonant/SKILL.md`.
Start Pi in this repository and use `/skill:install-control-resonant`; use `/reload` after skill changes.

## Project

- [Testing and calibration](docs/TESTING.md)
- [Installation and rollback](docs/INSTALLATION.md)
- [Evidence and limitations](docs/ANALYSIS.md)
- [Implementation plan and release gates](docs/PLAN.md)
- [Change history](docs/CHANGELOG.md)
- `src/`, `tests/`, `scripts/`: implementation and verification.
- `assets/dependencies.json`: pinned loader/menu archive hashes; no downloaded binaries are committed.
- `third_party/minhook/`: MinHook 1.3.4 source and license.
- `reference/`, `analysis/`, `build/`, `installations/`: ignored local research, generated output, and rollback data.

Default game folder: `G:\SteamLibrary\steamapps\common\CONTROL Resonant`.
The helper records the latest local receipt in `analysis/latest-installation.json`. Undo updates newest first.
Downloaded dependency redistribution permissions remain unverified; packaging is for this user's local installation.

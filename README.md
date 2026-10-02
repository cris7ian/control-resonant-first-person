# CONTROL Resonant — Exploration First Person

## Anchored preview: 0.3.0

A Windows x64 exploration-camera mod using CRLoader 1.0.0, CRModMenu 1.7.0, and Steam Input's Xbox layout.
The user confirmed the preceding offset view. This prototype now places the view relative to the player-following anchor instead of the retracted third-person position; live testing of the new placement is pending.

- Double-tap **RS/R3** or **K** in exploration or non-nested `story` areas to toggle the view.
- Combat clears the request. Double-tap again after combat; it never resumes automatically.
- Protected menus suspend the override. Focus loss clears the request.
- Change calibration in **Options → MODS → Exploration First Person (Prototype)**.

Defaults: **distance −6.35, height −0.15, side 0, fine forward −0.05**.
Distance now calibrates a fixed 6-unit reference, with range **−7 to −5**. Combined local offsets must stay within a 1.25-unit budget.
Updates preserve existing settings. The native defaults and menu reset values use the same calibration.

**This remains an experimental anchor-based preview, not a validated player-eye camera.**
Native boom shortening is no longer used for final placement. The native update and collision history still run unchanged.
Short eye-segment collision, visibility, camera ownership, and full combat coverage remain unvalidated.
Exact `story` state is newly allowed; active dialogue that retains `story` and mode 0 cannot yet be distinguished reliably. Detected protected states remain blocked.
FOV remains native. Geometry logging reports `anchor_valid`, `anchor_used`, and position-write results.
Enable **Diagnostic logging** for the short [wall test](docs/CAMERA.md). Toggle off or disable Enabled if the view clips or behaves incorrectly.
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
- [Camera collision and FOV investigation](docs/CAMERA.md)
- [Change history](docs/CHANGELOG.md)
- `src/`, `tests/`, `scripts/`: implementation and verification.
- `assets/dependencies.json`: pinned loader/menu archive hashes; no downloaded binaries are committed.
- `third_party/minhook/`: MinHook 1.3.4 source and license.
- `reference/`, `analysis/`, `build/`, `installations/`: ignored local research, generated output, and rollback data.

Default game folder: `G:\SteamLibrary\steamapps\common\CONTROL Resonant`.
The helper records the latest local receipt in `analysis/latest-installation.json`. Undo updates newest first.
Downloaded dependency redistribution permissions remain unverified; packaging is for this user's local installation.

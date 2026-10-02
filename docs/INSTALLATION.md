# Safe installation and removal

The current preview is **0.3.0**, an experimental anchor-based view derived from the calibrated offset checkpoint. It is not a validated player-eye camera.

## Prerequisites

- Windows x64 and the supported Steam executable.
- Python 3.10 or newer, CMake 3.20 or newer, Ninja, and WinLibs GCC/G++ on PATH.
- Local CRLoader 1.0.0 and CRModMenu 1.7.0 ZIP archives matching `assets/dependencies.json`.
- CONTROL Resonant closed. Hot unloading and DLL replacement while running are unsupported.

Supported executable SHA-256:
`4f6596b08bb5bc7fe4150cf5b9f71d7bae87eea02d66627c03a5e05ffe84ea62`.

Downloaded archives are not committed. Put them in `reference/archives/`, or supply `--dependency-dir`.
The helper also searches Downloads for matching loader/menu ZIPs. It never downloads or executes reference DLLs during verification.
Acquire the specified versions from the original mod distribution. Never bypass an archive hash mismatch.

## One-command installation

Run these commands from the repository root:

```powershell
python scripts/install.py
python scripts/install.py --apply
```

The first command builds, tests, stages fresh files, and prints a dry run. It does not change game files.
The second repeats verification, backs up replacements, installs mod files, and verifies installed hashes.
It also confirms that the executable and existing calibration settings remain unchanged.

Default game folder: `G:\SteamLibrary\steamapps\common\CONTROL Resonant`.
For another path:

```powershell
python scripts/install.py --game "D:\SteamLibrary\steamapps\common\CONTROL Resonant" --dependency-dir "D:\Downloads" --apply
```

If PATH selects the wrong compiler:

```powershell
python scripts/install.py --cc "<WinLibs>/bin/gcc.exe" --cxx "<WinLibs>/bin/g++.exe" --apply
```

No administrator rights should be needed. The helper never launches the game.
Launch through Steam after installation. Enable Steam Input with an Xbox layout.

## Repository install skill

Start Pi in this repository, then use `/skill:install-control-resonant`.
The skill lives in `.pi/skills/install-control-resonant/SKILL.md` and records this installation workflow.
Use `/reload` after adding or changing the skill in an active project session.

## Installation boundaries

Installed files are limited to:
- `winmm.dll` beside the game executable.
- `crmods/CRModMenu/` dependency files.
- `crmods/ExplorationFirstPerson/` our DLL, descriptor, safety INI, and MinHook license.
- `crmods.txt`, only when necessary to disable an existing BetterCamera reversibly.

A different existing loader/menu version blocks installation before writes. Reconcile it explicitly; do not overwrite it automatically.
Updates preserve existing ModMenu calibration. New installations and menu resets use the calibrated defaults in [TESTING.md](TESTING.md).
The safety INI controls camera writes at startup. Preserve a user's diagnostic-only setting rather than silently re-enabling writes.
The executable, archives, `steam_api64.dll`, saves, and unrelated mods remain unchanged.

Receipts and backups live under `installations/`. `analysis/latest-installation.json` records the latest installed version and receipt.
These local records are excluded from Git. Keep them for rollback.
Dependency staging is local-only; public redistribution permissions have not been verified.

## Remove or undo an update

Close the game first. Use the receipt printed by installation:

```powershell
python scripts/deploy.py uninstall --receipt "installations/<installation>/receipt.json"
python scripts/deploy.py uninstall --receipt "installations/<installation>/receipt.json" --apply
```

The first command is a dry run. The second removes unchanged added files and restores recorded backups.
Updates have separate receipts. Undo them **newest first**; undoing only the latest update restores the preceding preview.
New helper receipts link to `previous_receipt`. Older receipts require inspecting their timestamps and backups.

Removal preserves identical dependencies that predated installation. It preserves user-modified files and reports conflicts.
Do not delete conflicting files blindly. Game-generated logs and settings remain unless a specific recorded settings reset owns them.
Empty folders may remain. Never delete the whole `crmods` folder.

## Manual build and deployment

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER="<WinLibs>/bin/gcc.exe" -DCMAKE_CXX_COMPILER="<WinLibs>/bin/g++.exe"
cmake --build build
ctest --test-dir build --output-on-failure
python -m unittest discover -s tests -p "*_tests.py"
python scripts/package.py
python scripts/deploy.py install
python scripts/deploy.py install --apply
```

Low-level deployment prints its receipt but does not update the helper's latest-installation pointer.
Use `scripts/install.py` for routine installations, compatibility preflight, and pointer updates.
MinHook 1.3.4 is vendored under `third_party/minhook` with its license.
The DLL statically links the MinGW runtimes and does not require a local `libwinpthread-1.dll`.

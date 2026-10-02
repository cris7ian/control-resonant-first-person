---
name: install-control-resonant
description: Build, verify, and safely install or update this repository's CONTROL Resonant exploration first-person preview, including local CRLoader and CRModMenu dependencies. Use for installation, reinstallation, updates, or receipt-based rollback.
compatibility: Windows x64, Python 3.10+, CMake, Ninja, WinLibs GCC, and user-supplied hash-pinned dependency archives.
---

# Install CONTROL Resonant exploration first person

Resolve the repository root from this skill directory: `../../..`.
Read `../../../docs/INSTALLATION.md` before changing installed files.

## Install or update

1. Inspect the game path, existing mods, and `analysis/latest-installation.json`, if present.
2. Confirm the game is closed. Do not terminate it or attempt DLL hot replacement.
3. Confirm Python, CMake, Ninja, and WinLibs `gcc`/`g++` are available.
4. Run `python scripts/install.py` from the repository root.
5. Inspect the dry-run destinations and compatibility result.
6. If installation is requested, run `python scripts/install.py --apply`.
7. Report the version, verification results, preserved settings, and exact rollback receipt.

Use `--game "<game folder>"` when the installation differs from the default.
Use `--dependency-dir "<ZIP folder>"` if the archives are not under `reference/archives/` or Downloads.
Use `--cc "<WinLibs>/bin/gcc.exe" --cxx "<WinLibs>/bin/g++.exe"` if PATH selects the wrong compiler.

The helper builds and tests our DLL, stages fresh assets, verifies dependency hashes, and checks the supported executable twice.
The default command changes generated build files, not game files. Only `--apply` installs mod files.
Archives are local prerequisites, not committed or publicly redistributed dependencies.
Never bypass a hash mismatch, unsupported executable, conflicting loader/menu, or running-game guard.
Do not install BetterCamera alongside this camera hook.

## Preserve calibration and safety

Keep existing `ModMenuConfig/exploration_first_person.ini` settings during updates.
Missing settings use the compiled defaults; the descriptor uses the same defaults.
Current baseline: distance −6.35, height −0.15, side 0, fine forward −0.05; RS and keyboard K double-taps.
Placement uses a stable candidate anchor and a fixed 6-unit reference. Distance range is −7 to −5; combined local offsets are limited to 1.25 units.
Exact non-nested story areas are allowed. Active dialogue retaining story/mode 0 remains an unvalidated detection case.
Combat clears the request. Reactivation after combat always requires a new double-tap.
Do not reset settings unless requested. Back up requested resets and record hashes in a rollback receipt.
Do not silently turn `camera_writes` back on if the user disabled it for diagnostics.

## Roll back

Read the latest receipt and its `previous_receipt` link before removing files.
Use `python scripts/deploy.py uninstall --receipt "<receipt.json>"` to inspect a dry run.
Add `--apply` only when rollback is requested. Undo updates newest first.
For older receipts without links, inspect receipt timestamps and backup hashes; do not guess ownership.
Preserve user-modified files reported as conflicts. Do not remove the entire `crmods` directory.
Keep receipts and backups outside the game folder.

Never modify `CONTROLResonant.exe`, archives, saves, or `steam_api64.dll`.
Do not launch the game automatically. Tell the user to launch through Steam with Steam Input's Xbox layout.
Ask them to test RS toggles outside combat and manual reactivation after combat.
This is an anchored prototype, not a validated head-bone/eye-collision implementation. Native update/history and FOV remain unchanged.

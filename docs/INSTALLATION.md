# Installation and troubleshooting

## Release installation

**Close the game before changing mod files.** Hot replacement and DLL unloading are unsupported.

1. Open the game folder through Steam: **Properties → Installed Files → Browse**.
2. Install [CRLoader 1.0.0](https://www.nexusmods.com/controlresonant/mods/9) using its author's instructions.
3. Install [CRModMenu 1.7.0](https://www.nexusmods.com/controlresonant/mods/35) using its author's instructions.
4. Disable BetterCamera and any other camera-override mod.
5. Download the Windows x64 ZIP from [Releases](https://github.com/cris7ian/control-resonant-first-person/releases).
6. Extract it outside the game folder.
7. Copy `crmods/ExplorationFirstPerson` into the game's `crmods` folder.
8. Launch through Steam.

Do not copy the ZIP's root documentation into the game folder. Do not replace `steam_api64.dll` or the executable.
The release does not contain CRLoader, CRModMenu, game files, or personal settings.

Steam Input must provide an **Xbox layout** for controller input. The mod polls XInput rather than the physical controller directly.

### Verify the download

Each release includes `SHA256SUMS.txt`. Compare its ZIP hash with PowerShell output:

```powershell
Get-FileHash .\control-resonant-first-person-0.3.3-windows-x64.zip -Algorithm SHA256
```

The ZIP also includes `release-manifest.json`, with SHA-256 hashes for its payload files.

## Update without losing settings

1. Close the game.
2. Back up `crmods/ExplorationFirstPerson` outside the game folder.
3. Keep the existing `ExplorationFirstPerson.ini`.
4. Keep the existing `ModMenuConfig` folder.
5. Replace the DLL, menu descriptor, and notices from the new release.
6. Launch through Steam.

`ModMenuConfig/exploration_first_person.ini` contains menu calibration and controls. The mod creates it through CRModMenu.
`ExplorationFirstPerson.ini` contains the diagnostic safety switch. Do not overwrite it if camera writes were deliberately disabled.

## Supported game build

Version **0.3.3** supports both fingerprints below. Earlier builds support only the previous executable.
The updated executable requires 0.3.3; reinstalling an earlier build does not restore compatibility.
Version 0.3.3 remains a **pre-release**: the user confirmed first-person operation, but custom FOV was disabled during testing.

| Executable | SHA-256 | Size | Live validation |
| --- | --- | --- | --- |
| Previous build | `4f6596b08bb5bc7fe4150cf5b9f71d7bae87eea02d66627c03a5e05ffe84ea62` | 100,390,832 bytes | Camera, traversal, transitions, and FOV confirmed on 0.3.2 |
| Updated build, inspected October 10 | `9f56611d767c9227a8a1b250c01acf9478b50933c65f86a1e044fcc53df67cb1` | 99,470,256 bytes | User-confirmed first-person operation and logged manual toggles; custom FOV unconfirmed |

The exact hash, not the size alone, determines support. These fingerprints identify `CONTROLResonant.exe`.
Check it from the game folder:

```powershell
Get-FileHash .\CONTROLResonant.exe -Algorithm SHA256
```

The native adapter selects one complete address profile by exact fingerprint and checks hook signatures before installing hooks.
Source installation also verifies native references to the mode mirror, scratch camera, vtable, override flag, and FOV setter.
Version 0.3.3 retains the camera behavior and settings. The user confirmed working first-person operation on the updated executable.
The latest logs recorded eligible camera records, first-person activation, manual return, and protected-menu detection.
Custom FOV was disabled. No matched FOV write was recorded on this executable; the release remains a pre-release.
Disable **Custom first-person FOV** to use the confirmed position-only path. Traversal, combat, and focus coverage still need broader updated-build testing.
Previous release tags and assets remain unchanged.
After a game update, an unsupported executable leaves the camera untouched. Do not bypass the compatibility gate.

## Troubleshooting

### The mod menu is missing

- Check that CRLoader and CRModMenu are installed in the correct game folder.
- Check that the mod DLL and menu descriptor are directly under `crmods/ExplorationFirstPerson`.
- Check `crloader.log` in the game folder for loading errors.
- Restart after installing. A running game cannot discover a newly replaced DLL safely.

### The menu appears, but the camera does not change

- Enable the mod in **Options → MODS → Exploration First Person**.
- Try double-tapping **K** during exploration to separate controller input from camera issues.
- Release between taps; complete the second tap within 350 ms.
- Confirm Steam Input uses an Xbox layout.
- Check the supported executable hash and disable conflicting camera mods.
- Check `ExplorationFirstPerson.ini`: camera writes require `[Safety] camera_writes=1`.
- Return to the game window and double-tap again. Focus loss clears first-person intent.
- Check `crmods/ExplorationFirstPerson/ExplorationFirstPerson.log` for compatibility or hook errors.

Combat clears first-person intent. A new double-tap after combat is expected, not a failed automatic resume.
Protected menus and unsupported camera records can also prevent writes.

**Camera transition duration** defaults to 180 ms. **First-person horizontal FOV** defaults to 100°.
Version 0.3.2 permits first-person activation during wall traversal and rotates height/side calibration with the input pair.
Disable **Custom first-person FOV** to retain native FOV without disabling smooth position transitions.
Set transition duration to `0` for immediate manual toggles. Combat, focus loss, and protected-state returns remain immediate.
Changing settings clears intent; leave the menu and double-tap again.
If FOV does not change, inspect `Scoped FOV` logs. Unmatched render cameras retain native FOV, but position transitions remain available.
The user confirmed transitions, FOV, and wall traversal on the previous executable. Its logs recorded matched FOV writes for 100° and 120° settings.
Custom FOV on the updated executable still requires live confirmation, including a matched `Scoped FOV` write with custom FOV enabled.
This does not guarantee every native FOV effect or scripted camera sequence.

### The view clips or affects a conversation

Toggle off immediately. The final eye position has no separate collision sweep, and active conversations are not completely distinguishable.
Use the default calibration before reporting placement issues. Out-of-range settings or combined offsets beyond 1.25 units are ignored; the previous valid settings remain active.

### Collect diagnostics

1. Enable **Diagnostic logging** in the mod menu.
2. Reproduce the issue briefly.
3. Disable diagnostic logging.
4. Open an [issue](https://github.com/cris7ian/control-resonant-first-person/issues) with the mod version and reproduction steps.

Include relevant excerpts from `crloader.log` and `crmods/ExplorationFirstPerson/ExplorationFirstPerson.log`.
Mention other camera mods, controller layout, and executable hash. Remove personal paths or identifiers before sharing logs.
Do not upload saves, the executable, game archives, or dependency DLLs.

## Disable camera writes

Close the game, then edit `crmods/ExplorationFirstPerson/ExplorationFirstPerson.ini`:

```ini
[Safety]
camera_writes=0
```

Restart the game. This disables both position and FOV hooks while retaining input and state diagnostics.
To restore camera writes, close the game, set the value to `1`, and restart.

## Upload an update to Nexus Mods

Upload the generated public ZIP as version **0.3.3**:

```text
build/releases/v0.3.3/control-resonant-first-person-0.3.3-windows-x64.zip
```

The ZIP includes the mod DLL, menu descriptor, default safety INI, documentation, and required third-party notices.
Upload the complete ZIP, not individual DLLs or files copied from your installed mod folder.
Do not upload `build/package`, CRLoader, CRModMenu, logs, receipts, personal settings, or game files.
`SHA256SUMS.txt` is an optional verification attachment; it is not a required mod-installation file.

Label the file description **0.3.3 compatibility pre-release**. State that camera operation is user-confirmed, but updated-build custom FOV is unconfirmed.
Keep CRLoader 1.0.0 and CRModMenu 1.7.0 listed as separate requirements.
Tell existing users to close the game and preserve `ExplorationFirstPerson.ini` and `ModMenuConfig` when updating.

## Remove or roll back

**Close the game first.** Back up settings before removal if you want to reuse them.

For a manual release installation, remove only `crmods/ExplorationFirstPerson`, or restore your previous backup of that folder.
Leave CRLoader, CRModMenu, and other mods in place if they are still needed. Do not remove the whole `crmods` folder.

For installation through `scripts/install.py`, use the recorded receipt instead of deleting files manually:

```powershell
python scripts/deploy.py uninstall --receipt "installations/<installation-id>/receipt.json"
python scripts/deploy.py uninstall --receipt "installations/<installation-id>/receipt.json" --apply
```

The first command is a dry run. Undo updates newest first. Preserve any modified files reported as conflicts.
See [developer documentation](DEVELOPMENT.md#reversible-local-installation) for source-based installation.

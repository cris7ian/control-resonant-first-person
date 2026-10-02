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
Get-FileHash .\control-resonant-first-person-0.3.1-windows-x64.zip -Algorithm SHA256
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

Versions 0.3.0 and 0.3.1 support the executable named `CONTROLResonant.exe` with this SHA-256:

```text
4f6596b08bb5bc7fe4150cf5b9f71d7bae87eea02d66627c03a5e05ffe84ea62
```

The executable is 100,390,832 bytes. The exact hash, not the size alone, determines support.
Check it from the game folder:

```powershell
Get-FileHash .\CONTROLResonant.exe -Algorithm SHA256
```

The native adapter verifies the executable and expected hook signatures before installing hooks.
Version 0.3.1 has user-confirmed transitions and corrected FOV. Its existing download retains the pre-release label; stable 0.3.0 remains available.
After a game update, an unsupported executable leaves the camera untouched. Do not bypass the compatibility gate.

## Troubleshooting

### The mod menu is missing

- Check that CRLoader and CRModMenu are installed in the correct game folder.
- Check that the mod DLL and menu descriptor are directly under `crmods/ExplorationFirstPerson`.
- Check `crloader.log` in the game folder for loading errors.
- Restart after installing. A running game cannot discover a newly replaced DLL safely.

### The menu appears, but the camera does not change

- Enable the mod in **Options → MODS → Exploration First Person (Prototype)**.
- Try double-tapping **K** during exploration to separate controller input from camera issues.
- Release between taps; complete the second tap within 350 ms.
- Confirm Steam Input uses an Xbox layout.
- Check the supported executable hash and disable conflicting camera mods.
- Check `ExplorationFirstPerson.ini`: camera writes require `[Safety] camera_writes=1`.
- Return to the game window and double-tap again. Focus loss clears first-person intent.
- Check `crmods/ExplorationFirstPerson/ExplorationFirstPerson.log` for compatibility or hook errors.

Combat clears first-person intent. A new double-tap after combat is expected, not a failed automatic resume.
Protected menus and unsupported camera records can also prevent writes.

Version 0.3.1 adds **Camera transition duration** (180 ms default) and **First-person horizontal FOV** (100° default).
Disable **Custom first-person FOV** to retain native FOV without disabling smooth position transitions.
Set transition duration to `0` for immediate manual toggles. Combat, focus loss, and protected-state returns remain immediate.
Changing settings clears intent; leave the menu and double-tap again.
If FOV does not change, inspect `Scoped FOV` logs. Unmatched render cameras retain native FOV, but position transitions remain available.
The first installed preview applied no custom FOV in the captured session. The released correction fixes an expired stack-query check.
The user confirmed corrected transitions and FOV; live logs also recorded matched writes for 100° and 120° settings.
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

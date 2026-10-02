# CONTROL Resonant — Exploration First Person

Explore CONTROL Resonant in first person. Double-tap the right stick or **K** to switch views. Combat returns you to third person; switch back manually afterward.

**[Download 0.3.1 pre-release](https://github.com/cris7ian/control-resonant-first-person/releases/tag/v0.3.1)** · **[Stable 0.3.0](https://github.com/cris7ian/control-resonant-first-person/releases/tag/v0.3.0)** · **[Report a bug](https://github.com/cris7ian/control-resonant-first-person/issues)**

**0.3.1 pre-release:** fast eased transitions are user-confirmed in live play. The corrected, optional first-person FOV path still needs live retesting.

## Requirements

- Windows x64 and the supported Steam build of CONTROL Resonant.
- [CRLoader 1.0.0](https://www.nexusmods.com/controlresonant/mods/9) and [CRModMenu 1.7.0](https://www.nexusmods.com/controlresonant/mods/35), installed separately.
- For controller input: Steam Input enabled with an **Xbox layout**.

**Disable BetterCamera and other camera-override mods.** They can conflict with this mod's camera hook.

Game updates can change compatibility. The mod checks the executable and refuses camera hooks on unsupported builds. See [compatibility details](docs/INSTALLATION.md#supported-game-build).

## Install

**Close the game before installing or updating.** Do not replace DLLs while it is running.

1. Install CRLoader and CRModMenu using their authors' instructions.
2. Download `control-resonant-first-person-0.3.1-windows-x64.zip` from [Releases](https://github.com/cris7ian/control-resonant-first-person/releases).
3. Extract the ZIP into a temporary folder outside the game directory.
4. In Steam, open the game's **Properties → Installed Files → Browse**.
5. Copy the extracted `crmods/ExplorationFirstPerson` folder into the game's `crmods` folder.
6. Launch the game through Steam.

The result should look like this:

```text
CONTROL Resonant/
├── CONTROLResonant.exe
├── winmm.dll                         ← CRLoader, downloaded separately
└── crmods/
    ├── CRModMenu/                    ← downloaded separately
    └── ExplorationFirstPerson/
        ├── ExplorationFirstPerson.dll
        ├── ExplorationFirstPerson.ini
        ├── exploration_first_person.menu.json
        ├── MinHook-LICENSE.txt
        ├── THIRD-PARTY-NOTICES.md
        └── licenses/
```

The release ZIP includes this mod only. You do **not** need Python, CMake, or a compiler to install it.

**Updating?** Back up the existing mod folder first. Keep your existing `ExplorationFirstPerson.ini` and `ModMenuConfig` folder. Replace the DLL, menu descriptor, and notices only.

See [installation, troubleshooting, and removal](docs/INSTALLATION.md) for details.

## Controls and settings

| Action | Default |
| --- | --- |
| Toggle first person | Double-tap **RS/R3** or **K** |
| Open settings | **Options → MODS → Exploration First Person (Prototype)** |
| Return to third person | Double-tap again, or disable **Enabled** |

Release the button between taps. Complete the second tap within **350 ms**; holding the button does not toggle.

- Toggle in exploration and ordinary story areas, including dialogue-capable locations.
- Combat clears first-person intent. It does **not** automatically resume after combat.
- Protected menus suspend the camera override. Changing window focus clears first-person intent.
- Manual toggles ease over **180 ms**. Set **Camera transition duration** to `0` for instant switches.
- **First-person horizontal FOV** defaults to **100°**, with a 60–120° range. This option remains experimental in 0.3.1.
- Disable **Custom first-person FOV** to retain native field of view (FOV) without disabling eased movement.
- FOV matching failures retain native FOV and do not disable first-person placement.
- Combat and safety interruptions remain immediate. Changing settings clears intent; leave the menu and double-tap again.
- Keep **Diagnostic logging** off during normal play. Enable it briefly to investigate problems.

The default calibration is **distance −6.35, height −0.15, side 0, fine forward −0.05**. Adjust it in the mod menu. Positive fine forward moves the view forward; negative moves it backward. Distance uses a fixed reference, not the current collision-shortened camera boom.

## Known limitations

This is an early, anchor-based camera mod, not a head-bone camera or a combat first-person overhaul. The 0.3.0 placement baseline and 0.3.1 transitions are user-confirmed in live play.

- The first installed 0.3.1 preview retained native FOV because its lifetime check rejected every captured FOV write. This pre-release corrects that check; actual FOV application and native FOV effects still need live confirmation.

- Native camera collision still runs, but there is no separate collision sweep for the final eye position. Wall or body clipping remains possible.
- The mod does not hide the player's head or body.
- Active conversations that retain the ordinary story camera state are not reliably distinguishable. Toggle off if a conversation camera is affected.
- Combat and protected-state detection cannot guarantee coverage of every encounter or scripted sequence.

If the camera behaves incorrectly, toggle off or disable **Enabled**. See the [diagnostic safety switch](docs/INSTALLATION.md#disable-camera-writes).

## Development

See [developer documentation](docs/DEVELOPMENT.md) for building, testing, reversible local installation, and release packaging. See [change history](docs/CHANGELOG.md) for previous versions.

CRLoader and CRModMenu remain separate downloads. MinHook and compiler-runtime notices are included; see [third-party notices](THIRD-PARTY-NOTICES.md). No project-wide source license has been selected.

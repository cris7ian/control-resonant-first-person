# Reference analysis

## Current implementation baseline

Steam updated the executable after the initial inspection below. The diagnostic adapter now supports only SHA-256 `4f6596b08bb5bc7fe4150cf5b9f71d7bae87eea02d66627c03a5e05ffe84ea62`.

Current static evidence:
- Camera prologue candidate remains at RVA `0x207BF90`.
- Camera-mode setter resolves to RVA `0x1BCEC30`; its int32 mirror is RVA `0x5D05058`.
- The native UI state-to-fact mirror system is at RVA `0x17D8E10`.
- Its two observed arguments are FactDictionary and UIStateStacks references, using Windows x64 ABI.
- The diagnostic identity detour preserves the original update, then reads bounded UI stack snapshots.
- Native UI stack records have stride `0xF0`; state strings have stride `0x28`.
- Combat can exist beneath a protected top state. The observer records base/top states and combat presence.
- The explicit activity byte at `+0xB8` is only one activation source. Native RVA `0x17D8680` also inherits activation from active parent stacks whose current state names the target stack.
- Since 0.2.1, the bounded decoder mirrors this recursion with cycle protection and checks decoded activity dependencies in both snapshots. Live logs confirm inherited game activation.
- An internal native FPS component exists, but playable-character use and eye ownership remain unproven.

`analysis/game-baseline-updated.json` records the current executable. The older baseline farther below is historical only, not a current compatibility gate.

The implementation has passed synthetic tests and a native startup test in a separate unsupported host. That startup test executed only our compiled DLL, not the downloaded reference DLLs.

The user requested and tested a playable prototype. The original-first camera detour forwards ten observed integer/pointer argument slots and changes only the plausible record's position floats. Its compiled forwarding was inspected; full ABI and player-camera ownership remain unvalidated.

The first live session exposed a false exploration block: the game stack had explicit activity zero but inherited activation through `program_flow`. Version 0.2.1 corrected that check. Version 0.2.2 extended the distance range from −4 to −12.

The user then confirmed a useful exploration view at distance −6.3, height −0.2, side approximately zero, and fine forward −0.05. Version 0.2.3 makes these values the defaults, including keyboard K, and adds a repeatable installer and repository skill. These values are calibration, not proof of a player-eye anchor.

Logs confirm recognized gestures, eligible records, exploration activation, and pause/options/focus blocking. Reliable combat coverage, character visibility, exact units, and camera ownership still require testing. The user reported that native third-person wall retraction pushes the calibrated view forward; the older fixed post-update offsets retained that native displacement.

The dependencies and preview are installed locally with receipt-based backups. The executable remained unchanged. FOV and visibility remain native; collision is not disabled. Set the installed safety INI's `camera_writes=0` and restart for state/input diagnostics without the camera hook.

Static follow-up identified a native sweep/recovery resolver at RVA `0x2853140`, called twice by the camera update. Its mixed register/stack ABI differs from the preview's main camera hook. Version 0.2.4 adds opt-in candidate input/output telemetry to the existing hook only; it does not intercept or bypass this resolver. Candidate input records use owner `+0x30`, stride `0x30`. The user's subsequent capture provided 123 readable samples, including stationary free-space and strongly retracted positions while input0 stayed stable. See [CAMERA.md](CAMERA.md) for the proof boundaries, wall test, and native-FOV recommendation.

Version 0.3.0 uses matched input0 plus a fixed measured free-space reference (boom 6.0, height 0.05, side 0.10), rather than native output position. It preserves the updated distance −6.35 and height −0.15. All 123 rows passed replay through the compiled core; free-space calibration differed by at most 0.021 units after the height change. The stationary wall replay stayed 0.335 units from the anchor. These are replay results, not live validation of 0.3.0. Owned-record hook checks separately verify original forwarding and inactive/invalid-anchor no-write behavior without executing the game.

Exact non-nested story state in mode 0 is now allowed by request. The prior logs showed rejected double-taps in that state. Static reference inspection did not establish that active conversations always leave story/mode 0, so complete dialogue exclusion remains unproven. Local input plausibility and a 1.25-unit placement budget mitigate bad data, but do not prove head ownership or provide an eye collision sweep.

## Initial inspection method and limits

Inspected the four local ZIP files, extracted their contents, read the English author guide, and inspected the supplied JavaScript.

Inspected BetterCamera's native x64 DLL using PE metadata, ASCII/UTF-16 strings, and LLVM disassembly. Matched its camera prologue against the installed game file.

This is not a source-code review of BetterCamera: its package contains no source. Binary addresses below describe this exact reference DLL, not a stable API.

During the initial reference inspection, no DLL was executed, injected, or installed. No gameplay or camera behavior was tested. The game was not running during that process check.

Direct access to the supplied Nexus page returned HTTP 403. The web-search tool also failed. Website descriptions, source links, permissions, and support claims remain unverified.

## Packages

| Package | Contents relevant to this project |
| --- | --- |
| BetterCamera archive labeled 1.1 | `BetterCamera.dll`, `BetterCamera.ini`, `bettercamera.menu.json`, third-party notices |
| CONTROL Resonant loader 1.0.0 | `winmm.dll`, loader README, empty mod folder instructions |
| CRModMenu 1.7.0 | `modsui.dll`, readable `modsui.runtime.js`, its menu descriptor, third-party notices |
| CRModMenu Author Guide 1.7.0 | English/Chinese guides and example descriptors |

Local inspection archive paths and hashes remain in ignored `analysis/archive-inventory.json`. Installation dependency hashes are committed in `assets/dependencies.json`. Downloaded archives and DLLs are not committed.

**Version discrepancy:** the BetterCamera ZIP filename says 1.1. The DLL reports `FileVersion`/`ProductVersion` `4.6.0.0` and the string `Better Camera 4.6 by PewCat`. Record both identifiers; do not assume which is the public release version.

## Loader contract

Sources: `reference/CRLoader-1.0.0/README-crloader.txt` and `crmods/README.txt`.

- Place the loader's `winmm.dll` beside `CONTROLResonant.exe`.
- The proxy forwards the original Windows multimedia calls.
- The loader loads DLLs in `crmods` and one subfolder per mod.
- Default DLL order is alphabetical.
- Optional `crmods.txt`, beside `winmm.dll`, specifies enabled/disabled DLLs and load order.
- `crloader.log` records loaded mods.

BetterCamera has no PE export directory and no CLR runtime header. It is a native x64 DLL, not a managed .NET assembly. Initialization through DLL attachment is likely; the full loader implementation has not been recovered.

## Menu contract

Source: the complete `AUTHOR_GUIDE_EN.md` included in the author-guide package.

- Each mod supplies its own UTF-8 `.menu.json` descriptor.
- Descriptors may sit beside the game, in `crmods`, or in a direct subfolder of `crmods`.
- Menu selections are stored beside the descriptor: `ModMenuConfig/<mod-id>.ini`, section `[Settings]`.
- Native mods must read or watch that INI themselves. The menu does not call native setting callbacks.
- `window.CMM.value()` is a JavaScript interface, not a native C interface.
- The `dll` descriptor field reports module loading only, not whether hooks work.
- ModMenu does not load another mod's DLL.
- DLL hot unloading is unsupported.
- Key code **267** represents RS/R3 in the descriptor system. It is not an XInput bit mask.
- XInput's `XINPUT_GAMEPAD_RIGHT_THUMB` is `0x0080`; direct PlayStation input requires HID report parsing.
- The menu captures/rebinds keys but does not provide gameplay double-tap detection or suppress the game's binding.
- `context: "gameplay"` supports binding-conflict checks. `conflict: "warn"` permits a player to accept a shared game binding.

Recommended layout uses CRLoader to load `crmods/CRModMenu/modsui.dll`, with `modsui.runtime.js` beside it. No second proxy DLL is needed.

## Readable code: gameplay-state discovery

Source: `reference/CRModMenu-1.7.0/crmods/CRModMenu/modsui.runtime.js`.

At lines 718–724, `mayOpenMods()` checks:

```js
stack.state(StateStacks_programFlowStack.name) === 'game'
stack.active('game')
String(stack.state('game')) === 'exploration' // or 'combat'
```

The function also checks challenge and gameplay menu data. Lines 711–751 describe the native hotkey counter and JavaScript polling endpoint.

**Confirmed:** shipped readable code explicitly distinguishes `exploration` and `combat` through the game's UI state stack.

**Not confirmed:** those states accurately represent every combat encounter; their availability outside this injected module; or an existing native API exposing them.

The script's first line says it is injected inside a verified globals module. A new external script cannot assume access to its lexical variables.

Native menu DLL strings also mention a resource-handler hook, chaining after another mod, a camera-mode setter, and `Local\ControlMods.ResourceHook.%lu`. These are discovery clues, not a documented hook ABI or mutex protocol.

This supports two investigation routes:
1. Find and validate a native state mirror for low-latency camera gating.
2. If necessary, inject a separate state reporter in the verified UI scope and expose a mod-owned resource endpoint.

Do not patch or replace the shipped menu runtime as the final integration. A resource-hook bridge needs chain/order/thread validation and must fail closed on missing or stale telemetry.

## BetterCamera configuration

Sources: its INI and menu descriptor.

- It exposes camera height, distance, and side offsets in metres.
- Zoom supplies separate offsets, toggle/hold operation, speed, and smooth transitions.
- Its distance setting is an offset to the game's own camera distance, not an absolute eye position.
- Default controller support is disabled. The INI controller binding is `none`.
- Documentation describes DualSense USB/Bluetooth input. The binary also contains a dynamically resolved XInput fallback.
- Zoom bindings are individual buttons, chords, or alternative chords. No double-tap setting is present.
- Input can be ignored in menus. Conversations can keep the original camera.
- Experimental collision changes can cause wall clipping.
- Experimental FOV override replaces gameplay FOV effects. It is not needed for initial first-person geometry testing.
- No combat-specific option or explicit first-person option appears in the supplied descriptor.

Absence of a setting is not proof that a binary contains no related internal behavior.

## BetterCamera binary findings

Sources: `analysis/BetterCamera.dll.pe.txt`, full disassembly, extracted strings, and selected excerpts.

- PE32+, machine x64, image base `0x180000000`.
- Native C++ runtime/type-information strings are present.
- MinHook is credited in the third-party notices. Its use is also consistent with the observed hook-creation code.
- The hook installer passes DLL address `0x18000BF60` as the camera detour and stores a trampoline at `0x180048940`.
- The detour first calls the original function. It then conditionally adjusts the resulting camera data.
- It reads a record-array base at camera-object offset `+0x38` and an index at `+0x68`; candidate record address is `base + index * 0x40`.
- That record is checked before modification. Exact record semantics still need runtime validation.
- It reads three floats at record offsets `+0x18/+0x1C/+0x20`, normalizes their vector, and uses it for distance displacement.
- It modifies three floats at `+0x24/+0x28/+0x2C`; the arithmetic is consistent with position, with `+0x28` receiving height adjustment.
- Side displacement uses the horizontal components. These observations suggest a camera basis and position, not a known player-eye anchor.
- Separate conversation-start and conversation-camera detours are installed.
- Named setting discovery includes `CameraSet: Idle CameraSet Time`, `Camera:Override FOV`, and `Camera:Override Hor Native Aspect Ratio FOV Value`.
- The resolver checks a known location, then scans the camera prologue. Multiple candidates are treated as ambiguous rather than guessed.

These findings support an independent camera hook. They do **not** establish a correct function prototype, a full structure layout, or a body-visibility interface. All three need validation before shipping.

## Historical installed-game baseline

Source: `analysis/game-baseline.json` and the Steam app manifest inspected during this session.

- Steam app: `3669870`.
- Installed build ID: `25600401`.
- Pending target build ID: `25673981`; an update was queued in the manifest.
- Executable SHA-256: `a2e8e57c86ea60f12de1fa628f8db12014eb296fb259449ea688df9c7ba1497a`.
- BetterCamera's 25-byte prologue:

```text
48 8b c4 4c 89 48 20 53 56 57 41 54 41 55 41 56 41 57 48 81 ec 90 03 00 00
```

It occurs once in the executable's `.text` section, at relative virtual address (RVA) `0x207BF90`.

The reference DLL's known camera RVA is `0x2050730`, which differs. Its scanning path is therefore relevant to this installed file.

A unique prologue match is a **candidate**, not compatibility verification. Confirm the function's surrounding instructions, calling convention, record layout, and active-player ownership. Recheck the fingerprint after Steam updates.

## Controller observations

Windows device enumeration lists a Bluetooth DualSense and a Bluetooth Xbox Wireless Controller. This does not establish which controller the player uses, whether either is currently connected, or the game's Steam Input configuration.

The user specified Steam Input enabled with an Xbox layout and manual reactivation after combat. Live logs subsequently showed XInput slot 0 connected and recognized RS gestures. This confirms the XInput path, not the exact Steam settings page or every controller scenario.

Use the XInput-facing controller path for this project. Do not also read physical HID input: one click could otherwise count twice.

## Source and redistribution

The BetterCamera package supplies dependency notices, not an identified license for BetterCamera itself. No source repository was found in the inspected strings or local package.

Keep the downloaded packages as local references. Do not redistribute their DLLs or copy recovered implementation into a release without checking permissions. Implement our own logic and retain notices for dependencies we actually distribute.

## Links

Supplied by the user; website descriptions not retrieved:
- https://www.nexusmods.com/controlresonant/mods/9?tab=description
- https://www.nexusmods.com/controlresonant/mods/35?tab=description

The archive filename associates BetterCamera with mod ID 41; this was not confirmed from the website.

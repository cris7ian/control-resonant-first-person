# Implementation plan

## Implementation checkpoint: calibrated preview 0.2.3

Implemented and automated-test verified:
- Controller-independent double-tap detector and fail-closed camera policy.
- XInput backend, controller latching, focus/reset handling, and settings reload.
- Bounded UI stack decoding with synthetic corrupt/torn-memory tests.
- Exact-build-gated native identity observer on the state-to-fact mirror system.
- Native diagnostic DLL, menu descriptor, package staging, deployment, backups, and rollback.

The user requested and tested a playable prototype instead of further test expansion. The original-first camera detour and adjustable offsets are installed with CRLoader and CRModMenu. Version 0.2.1 fixed inherited activity; 0.2.2 expanded distance to −12. Version 0.2.3 adopts the user's working calibration and adds a one-command installer and repository install skill. The executable remains unchanged.

Preview activation requires a supported executable, readable/fresh UI snapshot, foreground input, native camera mode 0, and a structurally plausible writable camera record. Combat clears the request. These checks are not proof of playable-character ownership or complete protection coverage.

The user confirmed a useful exploration view. Native wall retraction still shifts it; the next dependency is identifying the pre-collision pivot or a narrowly scoped boom-retraction control. Eye/camera ownership, body visibility, and complete combat/protected coverage remain open. FOV remains native until axis, units, aspect handling, and a scoped override are validated. This is not a completed first-person mod. The independent resource bridge remains uninstalled. Effective activity now mirrors the native parent recursion with cycle protection; the explicit byte alone is not used as the final activity result.

Full requested scope remains the outcome below. Unresolved camera geometry, visibility, and collision work is not treated as complete or removed from scope.

## Outcome

Build a standalone exploration first-person camera mod for CONTROL Resonant. Use CRLoader for loading and CRModMenu for settings.

Double-tap RS/R3 during exploration to toggle first person. Combat and protected camera states must use the game's third-person or scripted camera.

This is a first-person exploration view, not a first-person combat overhaul. Combat weapons, combat aiming, and first-person combat animations are outside the requested behavior.

Full scope includes reliable input, true eye-level placement, character clipping control, combat gating, protected states, settings, verification, installation, and removal.

## Confirmed behavior

- Target Steam Input enabled with an Xbox layout. Validate the actual runtime configuration during setup.
- Read RS through the game's XInput-facing controller path; do not read physical DualSense HID in parallel.
- Combat returns to third person and clears the first-person request.
- After combat, require another manual double-tap of RS. Never resume first person automatically.
- Double-tap again during exploration to exit first person.

The same double-tap gesture applies after combat; ordinary single-click game behavior remains unchanged unless testing identifies a conflict.

## Architecture

Planned implementation: C++20, Windows x64, CMake, MinHook, and a small controller-independent policy core.

| Component | Responsibility |
| --- | --- |
| `InputBackend` | Read one selected gameplay controller; normalize button edges |
| `DoubleTapDetector` | Recognize two short clicks using a monotonic clock |
| `GameplayStateReader` | Report exploration, combat, protected, or unknown with freshness |
| `CameraPolicy` | Decide whether first person is requested and permitted |
| `GameAdapter` | Resolve and validate build-specific functions and camera/player data |
| `CameraOverride` | Apply eye placement to the active player camera after the original update |
| `VisibilityGuard` | Prevent local head/body clipping without altering other actors or world shadows |
| `SettingsStore` | Load the descriptor's INI defaults and validated live updates |
| `Diagnostics` | Record build, hook status, state transitions, and input backend |

Keep policy logic independent of Windows and game memory. Test it with synthetic controller events and state sequences.

The game adapter owns all reverse-engineered offsets. Do not spread constants through input, settings, or policy code.

A single component owns camera writes. Publish immutable policy/settings snapshots to the camera callback; do not read files or lock UI resources there.

### State policy

`effective_first_person = requested_first_person && exploration_confirmed && player_camera_valid && input_context_safe`

| Event | Proposed behavior |
| --- | --- |
| Valid double-tap in exploration | Toggle the request |
| Double-tap in combat or protected/unknown state | Ignore it and clear the tap sequence |
| Combat begins | Clear the request; restore game camera and character visibility |
| Combat ends | Stay third person until another double-tap |
| Menu, dialogue, cutscene, or photo mode begins | Suspend override and clear input history |
| Protected state ends | Resume only if the exploration request is still retained and state is confirmed |
| Loading, death, save reload, or player replacement | Clear request and invalidate cached player data |
| Focus lost, controller disconnect, stale state, invalid camera | Stop override, restore visibility, clear tap history |
| Mod disabled | Clear request and restore all owned temporary changes |

Post-combat reactivation is manual only. Do not add automatic-resume or combat-override settings.

## Ordered implementation

### 1. Establish a repeatable baseline

1. Use the confirmed Xbox-layout input and manual post-combat reactivation policy.
2. Verify Steam Input is enabled and record the actual RS gameplay binding.
3. Record the game fingerprint after any queued update completes.
4. Build an x64 DLL that logs startup without game hooks.
5. Validate CRLoader loading and one harmless CRModMenu setting.

Use a dedicated `crmods/ExplorationFirstPerson` folder. Keep the loader and menu in their supplied layouts.

Before installation, inventory existing proxy DLLs and mod folders. Refuse overwrites unless backups and restore paths are recorded. Do not change `steam_api64.dll`, game archives, or saves.

**Gate:** loader log identifies our DLL; MODS shows its descriptor; disabling it restores the original experience.

### 2. Validate gameplay-state detection before moving the camera

The readable ModMenu script provides a concrete starting point: the game stack exposes `exploration` and `combat`.

Preferred route:
1. Locate the native state mirror or transition function corresponding to those UI states.
2. Log state transitions without changing memory.
3. Verify exploration, combat, escape menus, loading, dialogue, death, and scripted sequences.
4. Establish how camera mode distinguishes photo mode and other non-player cameras.

Fallback route if the native mirror cannot be validated:
1. Locate the verified UI globals module used by CRModMenu.
2. Inject an independent state reporter in the appropriate scope.
3. Send state to a mod-owned resource endpoint such as `coui://base/__efp_state__.json`.
4. Hand telemetry to the policy layer without calling camera/game functions on the resource thread.
5. Reject out-of-order or stale reports and reports from a replaced UI session.

Use a distinct endpoint prefix and chain unrelated resource requests to the previous handler. Investigate the observed shared resource-hook mutex before integrating; its protocol is not documented in the guide.

Do not modify the dependency's runtime file as the release solution. Descriptor configuration alone cannot deliver gameplay telemetry.

For a bridge prototype, start with 50 ms reporting and a 250 ms freshness limit. Tune from measurements, not assumptions. A bridge may introduce visible detection delay; replace it with native signaling if combat rollback is not sufficiently prompt.

Input/intent processing must consume a fresh state before allowing first person. A timer on a live policy thread must invalidate stale telemetry even when camera updates stop.

**Gate:** every tested combat encounter produces a reliable blocked state. Unknown or missing telemetry never permits first person. Do not release an exploration-only mod with unverified combat detection.

### 3. Validate the camera hook and player anchor

Use the current unique camera prologue match as a candidate, not a hardcoded promise.

1. Confirm the candidate's instructions and complete calling convention.
2. Validate its record-array and current-record interpretation in the running game.
3. Identify which invocation owns the active player camera.
4. Begin with read-only logging and an identity detour.
5. Test a small reversible offset only after the identity detour remains stable.
6. Find the player root, eye anchor, or equivalent stable attachment.

The reference DLL suggests record stride `0x40`, a basis-like vector at `+0x18`, and position-like floats at `+0x24`. Validate all of these per supported build.

A camera moved forward by a fixed offset is only a geometry experiment. It is not the final first-person implementation: native third-person distance changes with movement, walls, and scripted camera behavior.

Final placement should use a validated player-eye anchor and the game's look orientation. If a head attachment is animated, measure its motion and derive a stable root-relative eye target where needed.

Keep the original update and native third-person behavior. Modify only the final active-player camera during allowed exploration.

**Gate:** validated camera ownership and structure, with no changes to menu, conversation, or cutscene cameras.

### 4. Implement controller input and double-tap logic

- Use XInput with Steam Input enabled and an Xbox layout.
- Do not implement or poll native DualSense HID for this target configuration.
- Select exactly one XInput controller. Physical and virtual device enumeration must not duplicate clicks.
- Latch the selected device; reset detection when it changes or disconnects.
- Do not use keyboard emulation or global remapping as the production input path.

Initial detector defaults:
- A tap is a press followed by release within 250 ms.
- Two completed taps must be within a 350 ms release-to-release window.
- Reject implausible edge bounce; initial minimum gap is 40 ms.
- A held button counts once and cannot complete a second tap.
- Consume each completed pair; three taps produce one toggle, not two overlapping toggles.

Use monotonic wall-clock time, not frame counts or game time. Reset history on focus loss, protected state, combat, disconnect, and settings/binding changes. Require a fresh release before arming when entering exploration with RS held.

Descriptor key code 267 is RS/R3. Translate it to the backend's representation; never treat descriptor values as XInput masks.

Preserve ordinary game input initially. Check whether either click invokes a conflicting RS action. If consuming the gesture is necessary, validate a game-input interception point and document the single-tap delay needed to distinguish it. Do not silently disable an existing gameplay action.

**Gate:** correct double-taps work on the chosen controller path, holds never toggle, and menus/combat/focus changes cannot complete an old gesture.

### 5. Deliver usable first-person exploration

1. Apply the validated eye target and retain native look controls.
2. Remove residual over-the-shoulder side displacement.
3. Tune position during walking, sprinting, crouching, and vertical movement.
4. Smooth normal toggles without frame-rate-dependent interpolation.
5. Restore vanilla behavior on combat and protected-state entry.
6. Validate local-character visibility near the eye position.

Do not position directly on an animated head bone if it causes animation-driven roll or excessive movement.

Investigate the game's near-camera fade first. If it is insufficient, implement a camera-local character/head visibility override. Restore all original flags on third-person return and player replacement. Preserve world shadows and other actors wherever the engine supports it; verify rather than promise this capability.

Keep camera collision protection. Validate near walls, corners, ceilings, and narrow passages. Use a safe swept/raycast-adjusted eye placement if the engine exposes one. Do not ship BetterCamera-style global obstacle ignoring to solve eye placement.

Leave FOV at the game's value initially. An optional exploration-only FOV setting can follow calibration, with original values restored in all blocked states. Determine horizontal versus vertical FOV before exposing a slider.

Combat rollback should stop first-person writes on the next camera update after the authoritative combat signal. Avoid smoothing through the character model on forced rollback. Measure upstream detection latency separately.

**Gate:** no head obstruction, unexpected roll, wall penetration, or persistent camera/visibility change after exit.

### 6. Add settings, diagnostics, and packaging

Provide a separate `exploration_first_person.menu.json` with its own mod ID and DLL filename.

Initial settings:
- Enabled.
- Controller button (default RS/R3, code 267).
- Keyboard fallback binding, calibrated to double-tap K (code 75); it can be unbound.
- Double-tap window and maximum tap duration.
- Eye height and forward calibration offsets within validated ranges.
- Normal-toggle transition duration.
- No automatic post-combat resume option; manual reactivation is required.
- Diagnostic logging, disabled by default.

Mark gameplay bindings with the appropriate context and warn about shared game bindings. The menu cannot recognize that our binding is a double-tap; explain that in the description.

Read `ModMenuConfig/exploration_first_person.ini` beside our descriptor. Validate finite values, ranges, and cross-setting constraints; retain the last valid snapshot on incomplete writes. Declare restart requirements for settings that cannot safely reload.

Log the executable fingerprint, validated adapter, hook failures, selected input path, state-source freshness, and forced rollbacks. Avoid high-volume per-frame logs and do not log controller identifiers unnecessarily.

Keep BetterCamera disabled while our camera hook is active. Both would otherwise modify the same camera path, and load order is not sufficient evidence of compatibility.

Distribute our files only, with installation links and dependency versions. Include licenses for dependencies we ship. Do not bundle the downloaded reference DLLs without verified permission.

### Joint safe installation

The user authorized local installation and testing of the labeled prototype, including dependencies. Keep that prototype designation until release gates pass. Do not describe reference-derived offsets as a completed player-eye camera.

1. Build and pass automated checks before staging the release.
2. Close the game before changing loader or mod files.
3. Recheck the executable fingerprint and supported adapter.
4. Inventory existing mods, proxy DLLs, load-order files, and relevant settings.
5. Produce a dry-run file list and verify package hashes against the recorded archives.
6. Back up every existing file that the installation would replace.
7. Install CRLoader and CRModMenu only where absent or explicitly reconciled.
8. Install our DLL and descriptor in their dedicated folder.
9. Disable BetterCamera reversibly if it is installed; preserve its settings and files.
10. Record installed hashes and restore paths in a rollback manifest.
11. Launch through Steam with the user and inspect loader and mod logs.
12. Test double-taps, exploration view, combat rollback, and manual reactivation together.
13. Restore the previous installation immediately if any safety gate fails.

No admin rights, game-archive patching, save modification, or `steam_api64.dll` replacement should be needed. Uninstall must remove only unchanged files installed by this project and restore recorded backups. Preserve files the user edited after installation and report the conflict instead of overwriting them.

Runtime discovery requires a separate diagnostic session before final release validation. Start that session with read-only telemetry or an identity camera detour, not an unvalidated camera-writing build.

**Gate:** settings apply as documented; invalid config fails safely; install/remove operations touch only recorded mod files.

## Current repository layout

```text
CMakeLists.txt
src/{core,native,game_adapter,state_snapshot,state_observer,camera_override}.*
assets/{exploration_first_person.menu.json,ExplorationFirstPerson.ini,dependencies.json}
tests/{core_tests.cpp,native_smoke.cpp,deploy_tests.py,package_tests.py}
scripts/{install,package,deploy}.py
.pi/skills/install-control-resonant/SKILL.md
docs/{ANALYSIS,PLAN,TESTING,INSTALLATION,CHANGELOG}.md
third_party/minhook/               # vendored source and license
reference/                         # ignored local dependency archives and research
analysis/                          # ignored local evidence and latest receipt pointer
build/                             # ignored generated binaries and package
installations/                     # ignored receipts and rollback backups
```

## Verification matrix

| Area | Required checks |
| --- | --- |
| Gesture | Valid pair, slow pair, bounce, hold, triple/quadruple taps, held during loading |
| Timing | 30/60/120+ FPS, frame stalls, clock continuity, focus loss |
| Input | Xbox layout with Steam Input enabled, selected XInput controller, reconnect, USB/Bluetooth where applicable, no duplicate clicks |
| Policy | Exploration entry, combat interruption, combat exit policy, repeated encounters |
| Scripted states | Dialogue, cutscene, loading, save reload, death, pause/options/map, photo mode |
| Movement | Walk, sprint, crouch, stairs, vertical traversal, exploration animations |
| Geometry | Narrow corridors, corners, ceilings, pitch limits, character clipping, shadows |
| Configuration | Missing INI, invalid values, partial writes, live changes, defaults reset |
| Integration | Loader/menu versions, resource hook order if used, BetterCamera disabled |
| Recovery | Mod disabled, missing hook, stale telemetry, unsupported build, invalid/player-replaced pointers |
| Persistence | No permanent camera flags, visibility flags, game archive changes, or save changes |

Automate gesture, policy, configuration, and signature-selection tests. Manual in-game validation is required for combat semantics, camera layout, visibility, and collision.

## Release criteria

- The selected controller reliably toggles first person with two short RS/R3 taps.
- First person cannot be enabled during combat or protected states.
- Entering combat reliably restores the original gameplay camera.
- Combat exit remains third person until a new manual RS double-tap.
- Exploration remains usable without character obstruction or wall clipping.
- Single-click behavior follows the documented conflict policy.
- Missing/ambiguous signatures and stale state fail safely with a useful log.
- Installation and removal are reversible without changing game archives or saves.

Remaining research gates are native combat-state access, validated player-eye/camera layout, and camera-local character visibility. The supplied mods reduce discovery work but do not prove these features already exist.

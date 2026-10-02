# Camera collision and FOV iteration

## Goal

Remove third-person boom retraction **only while first person is effective**.
Keep collision protection at the player's eyes. Preserve original third-person, combat, and scripted-camera behavior.
Keep the calibrated defaults while testing one camera variable at a time.

## Why a wall behind the player shifts the view

The current hook calls the native update, then offsets its resulting position.
The native update has already shortened the third-person camera boom to avoid the wall.
The same fixed offset therefore starts from a different position and pushes the first-person view forward.
More distance calibration cannot remove this changing base position.

Static inspection found a collision resolver at game RVA `0x2853140` for the supported fingerprint.
The camera update at RVA `0x207BF90` calls it at `0x207CE28` and `0x207CECB`.
The first result updates an intermediate position; the second supplies the final resolved camera position.
The helper performs physics sweeps and updates recovery history. The two calls have different inputs and flags.

The helper's observed Win64 slots include pointers in RCX/RDX/R8, a float in XMM3, and additional stack floats, flags, and pointers.
The existing ten-integer camera-detour declaration is **not** suitable for this helper.
No collision-helper detour is installed. Its full semantics and safe eye-collision integration remain unvalidated.

Named `Camera:Collision Near Fix` and `Camera:Collision Far Fix` values adjust sweep endpoints.
Zeroing them is not a verified collision-disable switch. Do not write shared global settings to fix first-person placement.
Local disassembly evidence remains under ignored `analysis/`; addresses are build-specific, not a public API.

## Safe direction

1. Correlate camera input candidates, native output, and visible wall retraction.
2. Identify a stable player pivot or pre-collision desired-camera position.
3. Place the view from that anchor instead of the shortened third-person boom.
4. Sweep the short pivot-to-eye segment, not the full third-person boom.
5. Stop first-person writes immediately when policy blocks them.

Keep the native update and collision history unchanged so third-person restoration remains native.
An uncollided boom endpoint plus a fixed offset is still an approximation, not a validated eye anchor.
Do not freeze a previous camera position; movement, rotation, teleportation, and loading would invalidate it.

## 0.2.4 geometry probe

The probe uses bounded reads inside the existing camera hook. It adds no native detour or collision bypass.
With **Diagnostic logging** enabled, it reads the input-array candidate at owner `+0x30`, index at `+0x68`, and stride `0x30`.
It samples the first two vector-like entries before and after the original update.
It also records the validated output record's native position, requested position, direction, and write status.
Input records are matched by index; failed reads appear as unavailable. These candidates are not assumed to be player-eye coordinates.

Publication uses nonblocking lock acquisition. The worker logs at most two samples per second; the camera callback does not write files.
With logging disabled, the extra input reads do not run. Position math, policy, and collision behavior remain unchanged.
This probe has compiled and passed startup checks, but its in-game geometry correlation still requires the test below.

### Live wall test

Keep distance −6.3, height −0.2, side 0, and fine forward −0.05 unchanged.
Stop if the camera clips, stutters, or behaves unexpectedly.

1. Launch through Steam and load an exploration area outside combat.
2. Enable **Diagnostic logging** in the MODS page.
3. Leave the menu and double-tap RS to enable the preview.
4. Stand in open space and keep yaw/pitch fixed for ten seconds.
5. Toggle third person and wait ten seconds without changing direction.
6. Return to first person and approach a wall behind the player.
7. Face away from the wall and keep yaw/pitch fixed for ten seconds.
8. Toggle third person there and wait ten seconds.
9. Move away, reactivate the preview, and check whether the view returns to normal.
10. Disable Diagnostic logging after capture.

Tell the assistant when the open-space and wall phases occur. It can read the local log; no upload is needed.
Look for `CAMERA GEOMETRY` lines. `write=ok; write_bytes=12` confirms the position write, not exclusive player-camera ownership.
The native/requested comparison tests the fixed-offset math. Input/output differences help identify the relevant anchor and retraction.
Do not implement a bypass until the free-space and wall samples establish the required data relationship.

## FOV recommendation

Keep native field of view (FOV) while fixing placement and wall behavior.
Moving the camera to the eyes does not inherently require a different FOV from third person.
A wider view can improve peripheral visibility but makes central objects smaller and increases edge stretching.
Comfort depends on display size, viewing distance, motion, and personal preference. There is no universal FPS-only angle.

The reference mod labels its FOV slider as **horizontal degrees** and defaults its override to off.
Its override explicitly replaces native gameplay FOV effects, including sprint effects. Its slider default of 90 is not the game's measured FOV.
The native setting name `Camera:Override Hor Native Aspect Ratio FOV Value` suggests aspect-dependent horizontal semantics.
It does not establish the reference aspect ratio, engine storage units, native value, or safe ownership/restoration rules.
The preview does not read or write an FOV setting yet.

For a symmetric perspective projection, with aspect ratio `a = width / height`:

`horizontalFov = 2 * atan(a * tan(verticalFov / 2))`

Thus horizontal and vertical degree values cannot be compared directly. For example, 90° horizontal at 16:9 is about 58.72° vertical.
These are projection examples, not known settings for CONTROL Resonant.

After eye placement is stable, an optional first-person-only FOV control can follow read-only projection measurements.
Keep **native** as the default. Preserve native per-frame effects instead of globally forcing a fixed value.
On combat, protected state, or third-person exit, let the native update generate its current FOV; do not restore a stale cached angle.

Local sources: the supported executable, `src/camera_override.cpp`, the reference menu descriptor, and BetterCamera setting-name strings.
External primary-source retrieval failed during this investigation. No external citation is treated as proof of this game's projection semantics.

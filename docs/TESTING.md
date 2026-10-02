# Playable preview test: 0.3.0

## Current evidence and limits

The user confirmed a useful exploration view after the activity fix and distance-range extension.
Logs show recognized double-taps, eligible camera records, and preview activation during exploration.
Pause/options and focus changes stop the override. This does not prove every protected state or combat encounter.

Version 0.3.0 uses the observed player-following input anchor and a fixed free-space reference. It no longer offsets the retracted third-person position.
The preceding live capture contained 123 geometry samples with readable inputs and 95 successful writes. The new anchored placement still needs live testing.
The anchor is not a proven head bone. Body clipping and eye-segment wall penetration remain possible; the 1.25-unit bound is not a collision sweep.
FOV stays native. Character visibility and global collision settings remain unchanged.
Exact non-nested `story` is now allowed. Mode changes and named protected states still block the view; active conversation retaining `story`/mode 0 remains a detection gap.

If the game crashes, stutters, or moves the camera unexpectedly, stop testing. Disable Enabled or use the diagnostic-only fallback below.

## Short test

1. Launch through Steam with Steam Input's Xbox layout.
2. Load a save outside combat.
3. Release RS, then double-tap it.
4. Double-tap again to restore third person.
5. Enable the preview and enter combat.
6. Confirm that combat restores the original camera.
7. Finish combat and confirm that the preview stays off.
8. Double-tap RS outside combat to reactivate it.
9. Open and close pause, options, and map screens.
10. Test walking, pitch changes, stairs, and walls behind the player.
11. Enter the previously blocked story area and try both toggle directions.
12. Start a conversation there and report whether the scripted camera remains native.
13. Stop testing if an active conversation is overridden; toggle off before continuing.

Single-click RS actions still reach the game. Report binding conflicts.
Keyboard fallback: double-tap **K**. Unbind it in the MODS page to use only RS.
Focus loss clears intent; double-tap again after returning to the game.
Protected menus can suspend a retained request; closing them can resume the preview. Combat never resumes it automatically.

## Calibrated defaults

Open **Options → MODS → Exploration First Person (Prototype)**.

| Setting | Default | Range |
| --- | --- | --- |
| Anchor distance calibration | −6.35 | −7 to −5 |
| Camera height offset | −0.15 | −0.5 to +0.5 |
| Camera side offset | 0 | −1 to +1 |
| Fine forward offset | −0.05 | −0.3 to +0.3 |
| Controller binding | RS/R3, code 267 | Supported pad binding |
| Keyboard binding | K, code 75 | Optional keyboard binding |
| Double-tap window | 350 ms | 150–800 ms |
| Maximum click duration | 250 ms | 50–500 ms |
| Minimum click gap | 40 ms | 10–100 ms |

The descriptor labels distance values in metres, following the reference. Engine scale and true eye ownership remain unverified.
These values come from the user's working calibration. Existing installations keep their settings during updates.

More-negative distance moves forward. Positive fine forward moves forward; negative fine forward moves backward.
Placement uses a fixed 6-unit boom reference plus measured 0.05 height and 0.10 side reference offsets, independent of native retraction.
Distance −6.35 and fine forward −0.05 give a +0.30 axial displacement from the anchor.
Combined axial, vertical, and lateral magnitudes must total no more than 1.25 units. Invalid settings keep the last valid calibration.
Use 0.05 increments. Leave the menu to check the view; double-tap if the preview is off.
If the view crosses the face or geometry, move distance toward −6 or disable the preview.

## Logs

Game folder: `G:\SteamLibrary\steamapps\common\CONTROL Resonant`.

- `crloader.log`: dependency and mod loading.
- `crmods/CRModMenu/modsui.log`, if produced: menu startup/errors.
- `crmods/ExplorationFirstPerson/ExplorationFirstPerson.log`: gestures, UI activity, camera eligibility, and preview transitions.

The mod log includes the version, executable fingerprint, explicit activity, and inherited effective activity.
A `CAMERA PREVIEW ON` message reports policy activation, not independent proof of camera ownership or every successful memory write.
With **Diagnostic logging** enabled, `CAMERA GEOMETRY` lines appear at most twice per second.
They record input vectors, native/requested positions, `anchor_valid`, `anchor_used`, and position-write status.
Input-anchor reads now run even when logging is off because placement depends on them. Additional pre-update debug reads remain optional.
See [CAMERA.md](CAMERA.md) for the wall-retraction test. Native collision history remains unchanged.
Tell the assistant when testing starts or finishes. It can inspect these local files; no upload is needed.

## Diagnostic-only fallback

1. Close the game.
2. Set `camera_writes=0` in `crmods/ExplorationFirstPerson/ExplorationFirstPerson.ini`.
3. Restart through Steam.

The observer and input logging remain available, but the camera hook is not installed.
Full release gates still require combat/protected-state validation, playable-camera ownership, stable eye placement, visibility, and collision handling.

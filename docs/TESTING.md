# Playable preview test: 0.2.3

## Current evidence and limits

The user confirmed a useful exploration view after the activity fix and distance-range extension.
Logs show recognized double-taps, eligible camera records, and preview activation during exploration.
Pause/options and focus changes stop the override. This does not prove every protected state or combat encounter.

The preview uses offsets from the native third-person camera, not a validated player-eye anchor.
Its native wall-retraction behavior still shifts the first-person view. Body clipping and wall penetration remain possible.
FOV stays at the game's value. The mod does not change character visibility or globally disable collision.

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

Single-click RS actions still reach the game. Report binding conflicts.
Keyboard fallback: double-tap **K**. Unbind it in the MODS page to use only RS.
Focus loss clears intent; double-tap again after returning to the game.
Protected menus can suspend a retained request; closing them can resume the preview. Combat never resumes it automatically.

## Calibrated defaults

Open **Options → MODS → Exploration First Person (Prototype)**.

| Setting | Default | Range |
| --- | --- | --- |
| Camera distance offset | −6.3 | −12 to 0 |
| Camera height offset | −0.2 | −0.5 to +0.5 |
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
Distance −6.3 and fine forward −0.05 give the same axial displacement as distance −6.25 and fine forward 0.
Use 0.05 increments for fine corrections. Leave the menu to check the view; double-tap if the preview is off.
Do not jump directly to −12. Move closer to zero if the camera crosses the face or geometry.

## Logs

Game folder: `G:\SteamLibrary\steamapps\common\CONTROL Resonant`.

- `crloader.log`: dependency and mod loading.
- `crmods/CRModMenu/modsui.log`, if produced: menu startup/errors.
- `crmods/ExplorationFirstPerson/ExplorationFirstPerson.log`: gestures, UI activity, camera eligibility, and preview transitions.

The mod log includes the version, executable fingerprint, explicit activity, and inherited effective activity.
A `CAMERA PREVIEW ON` message reports policy activation, not independent proof of camera ownership or every successful memory write.
Tell the assistant when testing starts or finishes. It can inspect these local files; no upload is needed.

## Diagnostic-only fallback

1. Close the game.
2. Set `camera_writes=0` in `crmods/ExplorationFirstPerson/ExplorationFirstPerson.ini`.
3. Restart through Steam.

The observer and input logging remain available, but the camera hook is not installed.
Full release gates still require combat/protected-state validation, playable-camera ownership, stable eye placement, visibility, and collision handling.

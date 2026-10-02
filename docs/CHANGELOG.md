# Change history

## 0.3.0 — anchored placement and story-area toggles

- Derive position from the matched player-following input anchor, not the shortened third-person boom.
- Preserve the free-space calibration with fixed reference boom 6.0, height 0.05, and side 0.10.
- Adopt the latest distance −6.35 and height −0.15; preserve the user's installed INI.
- Bound combined local calibration to 1.25 units and distance to −7…−5. Invalid inputs/configurations fail closed; no legacy fallback.
- Allow exact non-nested story states in mode 0, while retaining combat and detected protected-state guards.
- Add anchor/write telemetry and focused geometry/story regression checks.
- Keep native collision update/history, FOV, and visibility unchanged. Short eye collision and conversations retaining story/mode 0 remain live-test gaps.

## 0.2.4 — wall-retraction diagnostics

- Add opt-in bounded reads of candidate camera input records before/after the existing native update.
- Report native/requested positions and actual position-write status, at most twice per second.
- Keep geometry publication nonblocking and preserve the original native call and camera math.
- Add the free-space versus wall test and collision/FOV evidence notes.
- Do not change collision, native history, FOV, calibrated defaults, or existing settings. Live geometry correlation remains pending.

## 0.2.3 — calibrated baseline and repeatable installation

- Adopt the user's working calibration: distance −6.3, height −0.2, side 0, fine forward −0.05, keyboard K.
- Keep RS and manual post-combat reactivation. Preserve existing settings during updates.
- Add `scripts/install.py`: supported-build preflight, build, tests, staging, dry run, backups, and verification.
- Add the repository `/skill:install-control-resonant` workflow.
- Track pinned local dependency metadata without downloaded binaries or personal archive paths.
- Stage fresh assets rather than relying on a DLL rebuild to copy changed descriptors.
- Update live-test evidence, calibration guidance, install/remove procedures, and remaining release gates.
- Keep native FOV, character visibility, and collision. Wall retraction remains an open camera issue.

## 0.2.2 — distance range

Extend the native validator and menu distance range from −4 to −12. Keep previous calibration settings.

## 0.2.1 — inherited activity

Mirror native UI-stack parent activation. A zero explicit activity byte no longer incorrectly blocks active exploration.
Log effective activity and verify inherited activation and cycle handling with focused tests.

## 0.2.0 — installed camera-offset preview

Add the original-first position hook, adjustable offsets, state gating, XInput double-taps, diagnostics, and reversible local installation.
CRLoader 1.0.0 and CRModMenu 1.7.0 are local dependencies. BetterCamera stays disabled.

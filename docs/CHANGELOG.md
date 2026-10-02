# Change history

## 0.3.2 — wall traversal and confirmed camera features

- Allow first-person activation during wall traversal. Validate input-pair spacing rather than requiring world-Y alignment.
- Derive height and sideways placement from the matched pair's local-up candidate. Preserve floor calibration and native direction/roll.
- Keep the 1.25-unit local offset bound, 12-unit native-anchor bound, and all executable, signature, record, freshness, focus, and state gates.
- Confirm wall traversal through user live testing. The latest session retains eligible records and matched FOV writes without a camera rollback during traversal.
- Remove experimental/prototype labels from the menu, startup logs, installation metadata, and current documentation. Retain `prototype_distance` for settings compatibility.
- Avoid unused axis normalization during eligibility checks. Verify identical output for all 159 captured replay samples.
- Add rotated-placement, singular-view, invalid-pair, floor-return, FOV-match, safety-return, and stable-release metadata regressions.
- Correct the activation log to include eligible-state resume, not only double-taps. Remove stale FOV/traversal retesting notices.
- Retain known limits: no separate eye collision, no head/body hiding, and incomplete active-dialogue or scripted-state exclusion.

## 0.3.1 — transitions and experimental FOV pre-release

- Retain the live-tested 0.3.0 anchor placement and input/state gates. Do not use the archived 0.4.x player/physics decoder.
- Add 180 ms manual entry/exit transitions using quintic easing, current native/anchored endpoints, and continuous-position reversals. Expose duration from 0–500 ms.
- Keep combat, protected-state, focus, disabled-mod, invalid-record, and stale-control returns immediate.
- Add optional first-person horizontal FOV, default 100°, range 60–120°. Preserve native FOV changes after entry as additive offsets.
- Run the native projection builder unchanged first. Use the native matrix-rebuilding setter only on its validated scratch camera matching a fresh positioned record.
- Keep FOV rejection independent from position eligibility; do not change global FOV tweaks, CameraView FOV, or visibility.
- Confirm eased transitions through user live testing. Captured logs show the first installed preview applied no custom FOV.
- Fix a render-stage check that reread an expired native stack query. Revalidate the positioned output record instead; keep the scratch-camera, lens, record-content, freshness, and safety gates.
- Reject ambiguous render matches. Preserve the entry FOV reference across unrelated render-camera calls and reset it when the matched output record changes.
- Skip the second native projection rebuild when output FOV equals current native FOV. Never skip required writes based on the previous frame's output.
- Add regressions for expired queries, unrelated cameras, ambiguity, no-op FOV, forwarding, native effects, and immediate safety restoration.
- Do not log unmeasured FOV as zero degrees on rejected frames. Add a documentation ZIP-version regression and native lifetime guidance in AGENTS.md.
- Fix actual release repeatability by preserving the input PE timestamp during GNU strip. Verify repeated real ZIPs, not only mocked stripping.
- Initially publish as a pre-release pending corrected FOV retesting. Subsequent live logs record matched writes for 100° and 120° settings.
- The user then confirms the corrected release works. Merge the tested code into main and record that confirmation without moving tags or replacing published assets. Stable 0.3.0 remains available.

## 0.3.0 — first public release

- Confirm working anchored placement and story-area toggles through user live testing.
- Publish a mod-only Windows x64 ZIP with checksums, default assets, documentation, and third-party notices.
- Keep CRLoader and CRModMenu as separate downloads linked from their Nexus pages.
- Replace development plans and research notes with user installation guidance and maintained developer documentation.
- Add repository guidance, CI checks, and a separate public release builder.

- Derive position from the matched player-following input anchor, not the shortened third-person boom.
- Preserve the free-space calibration with fixed reference boom 6.0, height 0.05, and side 0.10.
- Adopt the latest distance −6.35 and height −0.15; preserve the user's installed INI.
- Bound combined local calibration to 1.25 units and distance to −7…−5. Invalid inputs/configurations fail closed; no legacy fallback.
- Allow exact non-nested story states in mode 0, while retaining combat and detected protected-state guards.
- Add anchor/write telemetry and focused geometry/story regression checks.
- Keep native collision update/history, FOV, and visibility unchanged. A separate eye collision sweep is not implemented; conversations retaining story/mode 0 remain ambiguous.

## 0.2.4 — wall-retraction diagnostics

- Add opt-in bounded reads of candidate camera input records before/after the existing native update.
- Report native/requested positions and actual position-write status, at most twice per second.
- Keep geometry publication nonblocking and preserve the original native call and camera math.
- Add the free-space versus wall test and collision/FOV evidence notes.
- Do not change collision, native history, FOV, calibrated defaults, or existing settings. Subsequent wall-test recordings informed 0.3.0 anchored placement.

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

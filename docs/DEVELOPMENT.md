# Development

## Build and test

The tested Windows toolchain is WinLibs GCC 15.2.0 with MinGW-w64 13.0.0, CMake, and Ninja.
Python 3.10+ is required for packaging and deployment tools. MinHook 1.3.4 is vendored as source.
Add the toolchain's `bin` folder to PATH.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build
ctest --test-dir build --output-on-failure
python -m unittest discover -s tests -p "*_tests.py"
```

Windows x64 builds produce `build/ExplorationFirstPerson.dll`. MinGW runtime libraries are linked statically.
Non-Windows builds run the portable core tests but do not produce the game DLL.

### Verification boundaries

- Core tests cover input gestures, policy, settings, coherent state snapshots, and anchored geometry.
- Native startup smoke tests verify unsupported-host fail-closed behavior, not game compatibility.
- Camera-hook smoke tests use owned records and a fake original function to check forwarding and guarded writes.
- Python tests cover deployment, dependency checks, asset staging, configuration preservation, and release packaging.
- `efp_geometry_replay` reads whitespace-separated anchor, secondary, native-position, and direction triples through standard input. Recordings remain local.

The 0.3.0 geometry replay passed all 123 captured wall-test samples. The user then confirmed the installed build works in live play.
Neither result proves complete camera ownership, eye collision, dialogue exclusion, or every combat transition.

For camera changes, test a wall behind the player, walking, pitching, stairs, both toggle directions, story areas, active conversation, combat rollback, and focus loss.
Stable 0.3.0 retains native FOV. The user confirmed corrected 0.3.1 transitions and FOV in live play.
The original installed preview logged no successful FOV writes. After the lifetime fix, live telemetry recorded matched writes for 100° and 120° settings.
The user then confirmed the corrected release works. Its tested code is merged into main; published tags and assets remain unchanged.
Do not claim real FOV application, native effects, render matching, or full sprint/conversation coverage solely because automated checks pass.

## Implementation map

| Files | Responsibility |
| --- | --- |
| `src/core.*` | Settings, completed double-taps, and camera intent policy |
| `src/state_snapshot.*`, `src/state_observer.*` | Bounded coherent UI-stack decoding and original-first observation |
| `src/game_adapter.*` | Exact executable/signature gates, camera mode, and state freshness |
| `src/camera_geometry.*` | Anchored placement and local displacement validation |
| `src/camera_transition.*` | Time-based quintic blend and manual-transition eligibility |
| `src/camera_override.*` | Original-first camera hook, record checks, final position writes, telemetry |
| `src/native.cpp` | DLL lifecycle, XInput worker, stable configuration reads, and logging |
| `assets/` | Default safety INI, menu descriptor, and pinned local dependency metadata |
| `scripts/` | Local installation/deployment and separate public release packaging |

## Camera contract

The camera update hook uses RVA `0x207BF90`. The state observer uses RVA `0x17D8E10`.
All RVAs apply only to the executable fingerprint in [installation documentation](INSTALLATION.md#supported-game-build).
The full native camera ABI and exclusive player-camera ownership remain provisional.

The hook forwards ten observed integer/pointer argument slots and preserves the original return value.
It runs the native update first, including native collision/history, then writes only 12 bytes of final output position.
It does not change direction, visibility, native camera history, or global collision settings.
Published 0.3.0 leaves FOV unchanged. Source 0.3.1 adds an independent original-first render projection hook; it never writes CameraView FOV or global tweaks.

Placement uses a validated, matched input0 record. It is a stable player-following candidate, not a proven head attachment.
The input/output indices, input pointer, coordinate plausibility, direction, private writable output, freshness, foreground, and native mode are checked.
Invalid or replaced anchors leave native output untouched. There is no legacy retracted-position fallback.

```text
target = anchor
       - normalizedDirection * (6.0 + distance - fineForward)
       + up * (0.05 + height)
       + horizontalRight * (0.10 + side)
```

Distance range is −7…−5. The combined absolute local offsets must remain within 1.25 units.
This bound limits calibration; it is not wall protection. A separate pivot-to-eye sweep and head/body hiding are not implemented.

State eligibility requires effectively active exploration, or exact non-nested `story`, with native mode 0.
Inherited stack activity follows case-insensitive parent dependencies with cycle protection and coherent double reads.
Combat clears intent; protected states suspend it; focus loss clears it. Dialogue retaining `story` and mode 0 remains ambiguous.

Defaults and menu reset values must agree: distance −6.35, height −0.15, side 0, fine forward −0.05; RS and keyboard K.

## Manual transitions and scoped FOV (0.3.1 pre-release)

Keep 0.3.0 placement and state detection. The archived 0.4.x ECS/ancestry/physics reader is not used.
The worker publishes a coherent settings/state/blend snapshot every input poll. Default duration is 180 ms; range is 0–500 ms.
Quintic easing uses `6t^5 - 15t^4 + 10t^3`, with zero velocity and acceleration at the endpoints.
A reversal starts at the current blend and scales duration by remaining distance. Position stays continuous; reversal velocity is not guaranteed continuous.
Blend each frame's current native output and current anchored endpoint. Do not freeze either endpoint in world space.
Manual exit eases out only while exploration remains eligible. Combat, focus loss, protected states, disabled settings, and stale/invalid data leave native output immediately.
Transition positions lie on the segment between the native camera and the bounded eye target; the intermediate segment is not limited to the target's 1.25-unit local budget.
No new eye collision or clipping guarantee is introduced.

The native render builder at RVA `0x1BD5660` takes `(camera, transform, float* fov, float aspect)` in RCX/RDX/R8/XMM3.
Call it unchanged first, once. Its caller at `0x1BD5240` uses scratch camera RVA `0x5D047C0`.
Validate that exact camera, writable lens storage, native vtable RVA `0x4835370`, perspective mode `+0x2CC == 1`, horizontal FOV `+0x2D0`, and aspect `+0x2D4`.
The getter at `0x3228290` computes vertical FOV as `2*atan(tan(horizontalFov/2)/aspect)`; native lens angles use radians.
Signature-check both builder and setter before installing the FOV hook. FOV hook rejection must not reject the existing position hook.

Match transform direction/position to exactly one of eight recent successfully positioned records, within 0.0001 units and 100 ms.
Reject ambiguous matches. Require the current control epoch and revalidate the output record's private writable memory, direction, position, focus, freshness, and native mode.
Do not reread the camera hook's query/owner pointer later. Native caller `0x208F050` constructs it at `RSP+0x50` and passes that stack address in RCX.
The stack query expires when its caller returns; the actual output-record address is the value retained for render correlation.
Regression tests fail against the original preview when that stack query is overwritten after the position write.
This remains a bounded render-record correlation gate, not proof of exclusive player ownership. Other native sequences still need live coverage.
FOV rejection leaves the native projection intact and does not invalidate position eligibility.

The optional horizontal first-person FOV defaults to 100 degrees, range 60–120. Capture native FOV on the first matched entry frame.
Apply `nativeFov + (requestedEntryFov - entryNativeFov) * positionedBlend` using native setter RVA `0x3228200`.
The setter receives its float in XMM1, updates `+0x2D0`, and calls the native projection rebuild at `0x3228DD0`.
Never hand-edit matrices or change input FOV, global override byte `0x5D04F90`, or shared game camera components.
Respect an active native global override by leaving FOV untouched. Preserve native lens/aspect parameters and native effects computed before the setter.
Key the entry reference by control epoch and matched output-record address. Unrelated render-camera calls do not clear it or replace gameplay telemetry.
A regression fails against the original preview when an unrelated camera call resets the native-effect reference.
Native FOV changes after entry remain additive. Activating during a native FOV effect includes that effect in the captured entry reference.
Skip the setter when output equals current native FOV within 0.000001 radians. Do not compare against the previous frame's modified FOV.
At completed exit or safety interruption, the next original render build supplies native FOV again. No persistent game FOV value needs restoration.

Change-only `Scoped FOV` logs report matching/rejection even without debug. Optional geometry includes native/output FOV and blend at most twice per second.
Rejected frames omit unmeasured angles rather than reporting zero-initialized values as 0 degrees. Disable diagnostic logging during normal play.
Owned-record tests cover expired stack queries, unrelated cameras, ambiguous matches, no-op projection rebuilding, mixed ABI, native FOV deltas, and safety returns.
Mocks and static evidence alone do not establish live FOV application. Corrected 0.3.1 now has matched live telemetry and explicit user confirmation.
This clears the main-branch merge gate, not the remaining ownership, clipping, native-effect, or scripted-state coverage limitations.

## Reversible local installation

This workflow builds and checks the mod, validates local dependency ZIPs, and records backups and rollback receipts.
It is separate from the dependency-free public release ZIP.

```powershell
python scripts/install.py --game "D:\SteamLibrary\steamapps\common\CONTROL Resonant"
python scripts/install.py --game "D:\SteamLibrary\steamapps\common\CONTROL Resonant" --apply
```

The first command is a dry run. Close the game before either command; never bypass the running-game or fingerprint guards.
The helper preserves calibration and the existing safety INI, including `camera_writes=0`.

`assets/dependencies.json` pins CRLoader 1.0.0 and CRModMenu 1.7.0 archive hashes.
Provide the original ZIPs through `--dependency-dir "<ZIP folder>"`, Downloads, or ignored `reference/archives/`.
For compiler selection, use `--cc "<toolchain>/bin/gcc.exe" --cxx "<toolchain>/bin/g++.exe"`.
No downloaded dependency binaries belong in Git or a public release.

Receipts and backups live under ignored `installations/`. The latest pointer is `analysis/latest-installation.json`.
Undo updates newest first; preserve modified-file conflicts. See [removal instructions](INSTALLATION.md#remove-or-roll-back).
The repository installation skill is `.pi/skills/install-control-resonant/SKILL.md`.

## Public release packaging

Build and run all checks first. Commit the intended source and documentation before packaging.

```powershell
python scripts/release.py --build build
```

The builder requires a clean Git tree, a Windows x64 DLL matching the project version, and GNU `strip` on PATH.
Use `--strip-tool "<toolchain>/bin/strip.exe"` to select the tool explicitly.
It strips a temporary copy, checks that runtime sections are unchanged, and leaves the original build untouched.
Set `SOURCE_DATE_EPOCH` to the input DLL's PE timestamp for GNU strip; otherwise BFD rewrites that timestamp from wall-clock time.
Verify repeat packaging with the real strip tool across different seconds, not only mocked strip calls.

Output under `build/releases/` contains a mod-only ZIP and `SHA256SUMS.txt`.
The ZIP includes default assets, documentation, third-party notices, and a hashed manifest tied to the source commit.
It never reads the dependency-inclusive `build/package` staging directory or installed game settings.

Use `v<version>` for the Git tag. Keep CMake and the menu descriptor version aligned.
Check README and installation ZIP examples against that version; a repository regression guards these examples.
For pre-releases, retain the stable main branch and tag the verified feature-branch commit. Run CI on that exact source before publishing.
Download published assets again and verify checksums, manifest source identity, and every payload hash.
Upload only the ZIP and checksum file. Do not upload local installation packages, dependency archives, telemetry, or receipts.

Update runtime notices when changing the compiler toolchain. The project's own source license has not been selected.

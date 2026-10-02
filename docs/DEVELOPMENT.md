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
Keep native FOV while testing placement. Do not claim all these cases passed solely because automated checks pass.

## Implementation map

| Files | Responsibility |
| --- | --- |
| `src/core.*` | Settings, completed double-taps, and camera intent policy |
| `src/state_snapshot.*`, `src/state_observer.*` | Bounded coherent UI-stack decoding and original-first observation |
| `src/game_adapter.*` | Exact executable/signature gates, camera mode, and state freshness |
| `src/camera_geometry.*` | Anchored placement and local displacement validation |
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
It does not change direction, projection/FOV, visibility, native camera history, or global collision settings.

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

Output under `build/releases/` contains a mod-only ZIP and `SHA256SUMS.txt`.
The ZIP includes default assets, documentation, third-party notices, and a hashed manifest tied to the source commit.
It never reads the dependency-inclusive `build/package` staging directory or installed game settings.

Use `v<version>` for the Git tag. Keep CMake and the menu descriptor version aligned.
Upload only the ZIP and checksum file. Do not upload local installation packages, dependency archives, telemetry, or receipts.

Update runtime notices when changing the compiler toolchain. The project's own source license has not been selected.

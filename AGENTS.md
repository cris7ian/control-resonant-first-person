# Repository guidance

## Scope

This repository implements a Windows x64 CONTROL Resonant exploration-camera mod.
Keep README.md user-focused. Put maintained technical details in docs/DEVELOPMENT.md.
Do not restore obsolete implementation plans, research dumps, or unused runtime bridges.

## Camera and input invariants

- Run the original native functions first. Preserve their arguments, return values, and collision history.
- Only change the validated final camera position. Do not globally disable collision or change FOV or visibility.
- Place the camera from the matched input anchor. Never fall back to collision-shortened output offsets.
- Preserve executable fingerprint, hook-signature, record, freshness, focus, and state gates. Fail closed on uncertainty.
- Treat input0 as a player-following candidate, not proven head ownership or exclusive camera ownership.
- Preserve bounded local calibration and matching native/menu defaults.
- Use XInput for Steam Input's Xbox layout. Do not poll physical HID in parallel.
- Combat clears intent. Post-combat reactivation must require a new completed double-tap.
- Do not claim complete dialogue exclusion. Ordinary story state can also appear during active conversations.

## Verification

Build with CMake and Ninja using a Windows x64 GCC toolchain:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build
ctest --test-dir build --output-on-failure
python -m unittest discover -s tests -p "*_tests.py"
```

Add focused regression checks for changed behavior. Mock records, replay, and unsupported-host startup do not replace live-game validation.
Keep behavior claims separate from implementation assumptions. Update documentation when defaults, compatibility, installation, or limitations change.

## Installation safety

- Close the game before installing. Never terminate it automatically, hot-replace DLLs, or unload the mod.
- Keep BetterCamera and conflicting camera mods disabled.
- Use scripts/install.py for source-based local installation. Its default is a dry run; --apply writes mod files.
- Preserve existing calibration, ModMenuConfig, and ExplorationFirstPerson.ini, including camera_writes=0.
- Never change CONTROLResonant.exe, game archives, saves, steam_api64.dll, or unrelated mods.
- Retain installation receipts and backups locally. Roll back newest first and preserve user-modified conflicts.
- Never commit dependency ZIPs, downloaded DLLs, logs, analysis, local receipts, or personal settings.

The repository installation skill is .pi/skills/install-control-resonant/SKILL.md.

## Releases and Git

- Use scripts/release.py for public artifacts. scripts/package.py is for local dependency-inclusive installation only.
- Public ZIPs must contain only this mod, documentation, and required third-party notices.
- Keep CMake, menu descriptor, release tag, and artifact versions consistent.
- Do not remove third-party copyright or license notices.
- Do not select or change the project's own license without the owner's approval.
- Verify before publishing. Do not rewrite published history or force-push without explicit approval.
- Do not add co-author trailers, bot signatures, or authorship attribution to Git metadata.

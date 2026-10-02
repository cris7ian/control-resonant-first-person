# Third-party notices

These notices apply to the named components, not to this project's own source code. No project-wide source license has been selected.

## MinHook 1.3.4

Source: https://github.com/TsudaKageyu/minhook/tree/v1.3.4

MinHook is compiled into ExplorationFirstPerson.dll. Its license includes the MinHook and Hacker Disassembler Engine copyright notices and disclaimers.
The complete text is in `third_party/minhook/LICENSE.txt` in the source repository and `MinHook-LICENSE.txt` in the released mod folder.

## GCC 15.2.0 runtime libraries

The Windows release uses WinLibs GCC with statically linked GCC/libstdc++ runtime code.
The runtime libraries are covered by GNU GPL version 3 with the GCC Runtime Library Exception, version 3.1.
The exception permits eligible compiled combinations with independent modules; it does not assign GPL to this project's own code.

Included verbatim texts:

- `licenses/gcc-gpl-3.0.txt`: https://github.com/gcc-mirror/gcc/blob/releases/gcc-15.2.0/COPYING3
- `licenses/gcc-runtime-exception.txt`: https://github.com/gcc-mirror/gcc/blob/releases/gcc-15.2.0/COPYING.RUNTIME

Upstream source: https://github.com/gcc-mirror/gcc/tree/releases/gcc-15.2.0
Toolchain distribution: https://winlibs.com/

## MinGW-w64 13.0.0 and winpthreads

MinGW-w64 runtime code and winpthreads are supplied by the Windows toolchain.
The MinGW-w64 distribution's notice identifies Zope Public License 2.1 and separately marked portions.
Winpthreads includes the mingw-w64 permission notice and the Lockless Inc. BSD-style notice.

Included verbatim texts:

- `licenses/mingw-w64.txt`: https://github.com/mingw-w64/mingw-w64/blob/v13.0.0/COPYING
- `licenses/mingw-w64-crt.txt`: https://github.com/mingw-w64/mingw-w64/blob/v13.0.0/COPYING.MinGW-w64-runtime/COPYING.MinGW-w64-runtime.txt
- `licenses/winpthreads.txt`: https://github.com/mingw-w64/mingw-w64/blob/v13.0.0/mingw-w64-libraries/winpthreads/COPYING

Upstream source: https://github.com/mingw-w64/mingw-w64/tree/v13.0.0

## Separate game-mod dependencies

[CRLoader 1.0.0](https://www.nexusmods.com/controlresonant/mods/9) and [CRModMenu 1.7.0](https://www.nexusmods.com/controlresonant/mods/35) are required separate downloads.
Their DLLs and archives are not included in this repository or its public release ZIP.
Refer to their authors' pages for their terms and installation instructions.

No game executable, game archives, Steam API DLL, or BetterCamera binary is distributed.

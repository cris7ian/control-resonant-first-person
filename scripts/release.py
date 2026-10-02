"""Create a mod-only public release from a verified Windows x64 build."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import tempfile
import zipfile

if __package__:
    from . import install, package
else:
    import install
    import package

ROOT = Path(__file__).resolve().parents[1]
MOD = 'crmods/ExplorationFirstPerson'
LICENSES = ('gcc-gpl-3.0.txt', 'gcc-runtime-exception.txt', 'mingw-w64.txt', 'mingw-w64-crt.txt', 'winpthreads.txt')
DOCS = ('README.md', 'AGENTS.md', 'THIRD-PARTY-NOTICES.md', 'docs/INSTALLATION.md',
        'docs/DEVELOPMENT.md', 'docs/CHANGELOG.md')


def runtime_sections(image: bytes) -> dict[str, bytes]:
    """Validate PE32+ DLL headers and retain every non-debug section for comparison."""
    if len(image) < 64 or image[:2] != b'MZ':
        raise ValueError('Expected a Windows x64 DLL')
    pe = struct.unpack_from('<I', image, 0x3c)[0]
    if pe + 26 > len(image) or image[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Invalid PE header')
    machine, count, _, symbols, symbol_count, optional_size, flags = struct.unpack_from('<HHIIIHH', image, pe + 4)
    if machine != 0x8664 or not flags & 0x2000 or optional_size < 2:
        raise ValueError('Expected an AMD64 DLL')
    if struct.unpack_from('<H', image, pe + 24)[0] != 0x20b:
        raise ValueError('Expected PE32+ optional header')
    table = pe + 24 + optional_size
    if count == 0 or count > 96 or table + count * 40 > len(image):
        raise ValueError('Invalid PE section table')
    strings = symbols + symbol_count * 18 if symbols else 0
    result = {}
    for i in range(count):
        offset = table + i * 40
        name = image[offset:offset + 8].rstrip(b'\0')
        if name.startswith(b'/'):
            try:
                start = strings + int(name[1:])
                end = image.index(b'\0', start)
                if not strings or start < strings + 4: raise ValueError('Missing PE string table')
                name = image[start:end]
            except (ValueError, OverflowError) as error:
                raise ValueError('Invalid PE section name') from error
        decoded = name.decode('ascii')
        size, position = struct.unpack_from('<II', image, offset + 16)
        if position + size > len(image):
            raise ValueError('PE section exceeds file bounds')
        if decoded.startswith('.debug'):
            continue
        if decoded in result:
            raise ValueError('Duplicate PE section')
        # Include virtual layout and characteristics, not file offsets altered by strip.
        result[decoded] = image[offset + 8:offset + 16] + image[offset + 36:offset + 40] + image[position:position + size]
    return result


def source_commit() -> str:
    status = subprocess.run(['git', 'status', '--porcelain'], cwd=ROOT, check=True, capture_output=True, text=True)
    if status.stdout.strip():
        raise RuntimeError('Commit the intended changes before creating a public release')
    return subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()


def make_release(build: Path, output: Path, strip_tool: str, revision: str) -> tuple[Path, Path]:
    version = package.project_version()
    native = (build / 'ExplorationFirstPerson.dll').read_bytes()
    before = runtime_sections(native)
    if (version + '\0').encode() not in native:
        raise ValueError('DLL version does not match CMake; rebuild before releasing')
    descriptor = json.loads((ROOT / 'assets/exploration_first_person.menu.json').read_text(encoding='utf-8'))
    if descriptor['version'] not in (version, version + '-preview'):
        raise ValueError('Menu version does not match CMake')
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='efp-release-', dir=output) as temporary:
        stripped = Path(temporary) / 'ExplorationFirstPerson.dll'
        stripped.write_bytes(native)
        # GNU BFD otherwise rewrites the PE timestamp from wall-clock time on every strip.
        # Preserve the input build timestamp so repeated packaging of the same DLL is reproducible.
        pe_offset = struct.unpack_from('<I', native, 0x3c)[0]
        timestamp = struct.unpack_from('<I', native, pe_offset + 8)[0]
        environment = {**os.environ, 'SOURCE_DATE_EPOCH': str(timestamp)}
        subprocess.run([strip_tool, '--strip-debug', '--strip-unneeded', str(stripped)], check=True, env=environment)
        release_native = stripped.read_bytes()
        if runtime_sections(release_native) != before:
            raise RuntimeError('Stripping changed runtime sections; refusing release')
    # Explicit allowlist. Never walk build/package, dependency archives, or installed settings.
    payload = {name: (ROOT / name).read_bytes() for name in DOCS}
    payload[f'{MOD}/ExplorationFirstPerson.dll'] = release_native
    for name in ('ExplorationFirstPerson.ini', 'exploration_first_person.menu.json'):
        payload[f'{MOD}/{name}'] = (ROOT / 'assets' / name).read_bytes()
    payload[f'{MOD}/MinHook-LICENSE.txt'] = (ROOT / 'third_party/minhook/LICENSE.txt').read_bytes()
    payload[f'{MOD}/THIRD-PARTY-NOTICES.md'] = (ROOT / 'THIRD-PARTY-NOTICES.md').read_bytes()
    for name in LICENSES:
        payload[f'{MOD}/licenses/{name}'] = (ROOT / 'licenses' / name).read_bytes()
    manifest = {'schema': 1, 'version': version, 'source_commit': revision,
                'supported_executable_sha256': install.supported_hash(),
                'files': [{'path': name, 'sha256': hashlib.sha256(data).hexdigest(), 'size': len(data)}
                          for name, data in sorted(payload.items())]}
    payload['release-manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    artifact = output / f'control-resonant-first-person-{version}-windows-x64.zip'
    with zipfile.ZipFile(artifact, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(payload.items()):
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    checksum = output / 'SHA256SUMS.txt'
    checksum.write_text(f'{hashlib.sha256(artifact.read_bytes()).hexdigest()}  {artifact.name}\n', encoding='utf-8')
    print(f'Created {artifact} ({artifact.stat().st_size:,} bytes); {len(payload)} allowlisted files')
    print(f'Checksum: {checksum}')
    return artifact, checksum


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/releases')
    parser.add_argument('--strip-tool', default=shutil.which('strip'))
    args = parser.parse_args()
    if not args.strip_tool:
        raise RuntimeError('GNU strip required; add the toolchain bin folder to PATH or use --strip-tool')
    make_release(args.build.resolve(), args.output_dir.resolve(), args.strip_tool, source_commit())


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))

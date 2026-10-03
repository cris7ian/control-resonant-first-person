"""Build, check, stage, and plan installation. Use --apply to change game mod files."""
from pathlib import Path
import argparse
import json
import re
import shutil
import subprocess
import sys

if __package__:
    from . import deploy, package
else:
    import deploy
    import package

ROOT = Path(__file__).resolve().parents[1]

def supported_hash() -> str:
    path = ROOT / 'src/game_layout.hpp'
    if not path.is_file(): path = ROOT / 'src/game_adapter.cpp'
    source = path.read_text(encoding='utf-8')
    match = re.search(r'supported_hash\[\]\s*=\s*"([0-9a-f]{64})"', source)
    if not match: raise RuntimeError('Cannot determine native compatibility gate')
    return match.group(1)

def check_game(game: Path) -> str:
    deploy.ensure_closed()
    executable = deploy.contained(game, 'CONTROLResonant.exe')
    if not executable.is_file(): raise RuntimeError(f'Game executable not found: {executable}')
    fingerprint = deploy.digest(executable)
    if fingerprint != supported_hash():
        raise RuntimeError(f'Unsupported executable SHA256: {fingerprint}. Do not bypass the native compatibility gate.')
    return fingerprint

def preserve_safety(staging: Path, game: Path) -> None:
    rel = 'crmods/ExplorationFirstPerson/ExplorationFirstPerson.ini'
    existing = deploy.contained(game, rel)
    if not existing.is_file(): return
    staged = deploy.contained(staging, rel)
    deploy.atomic_write(staged, existing.read_bytes())
    manifest_path = staging / 'package-manifest.json'
    manifest = json.loads(manifest_path.read_text())
    for item in manifest['files']:
        if item['path'] == rel: item['sha256'] = deploy.digest(staged)
    deploy.atomic_write(manifest_path, json.dumps(manifest, indent=2).encode())

def run(command: list[str]) -> None:
    print('+ ' + subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)

def install(args: argparse.Namespace) -> Path | None:
    game, build = args.game.resolve(), args.build.resolve()
    fingerprint = check_game(game)
    if build.is_relative_to(game): raise ValueError('Keep the build directory outside the game directory')
    package.dependency_archives(args.dependency_dir)
    cc = args.cc or shutil.which('gcc')
    cxx = args.cxx or shutil.which('g++')
    if not cc or not cxx: raise RuntimeError('WinLibs gcc/g++ required. Add its bin folder to PATH or pass --cc and --cxx.')
    cc = Path(shutil.which(cc) or cc).resolve().as_posix()
    cxx = Path(shutil.which(cxx) or cxx).resolve().as_posix()
    run(['cmake', '-S', str(ROOT), '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
         f'-DCMAKE_C_COMPILER={cc}', f'-DCMAKE_CXX_COMPILER={cxx}'])
    run(['cmake', '--build', str(build)])
    run(['ctest', '--test-dir', str(build), '--output-on-failure'])
    run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-p', '*_tests.py'])
    staging = package.stage(build, args.dependency_dir)
    preserve_safety(staging, game)
    check_game(game)  # Steam may have updated the file while building
    deploy.perform_install(staging, game, ROOT / 'installations', False)
    if not args.apply:
        print('Dry run complete. No game files changed. Repeat with --apply to install.')
        return None
    latest_path = ROOT / 'analysis/latest-installation.json'
    previous = json.loads(latest_path.read_text()) if latest_path.is_file() else {}
    if previous.get('game') != str(game): previous = {}
    config = deploy.contained(game, 'crmods/ExplorationFirstPerson/ModMenuConfig/exploration_first_person.ini')
    before_settings = deploy.digest(config) if config.is_file() else None
    receipt_path = deploy.perform_install(staging, game, ROOT / 'installations', True)
    manifest = json.loads((staging / 'package-manifest.json').read_text())
    for item in manifest['files']:
        if deploy.digest(deploy.contained(game, item['path'])) != item['sha256']:
            raise RuntimeError(f"Installed hash mismatch: {item['path']}; inspect the rollback receipt.")
    if deploy.digest(game / 'CONTROLResonant.exe') != fingerprint:
        raise RuntimeError('Game executable changed during installation; inspect the rollback receipt.')
    if (deploy.digest(config) if config.is_file() else None) != before_settings:
        raise RuntimeError('Settings changed during installation; inspect before continuing.')
    if receipt_path is not None:
        receipt = json.loads(receipt_path.read_text())
        receipt['previous_receipt'] = previous.get('receipt')
        receipt['version'] = manifest['version']
        deploy.atomic_write(receipt_path, json.dumps(receipt, indent=2).encode())
    info = {'receipt': str(receipt_path) if receipt_path else previous.get('receipt'),
            'previous_receipt': previous.get('receipt') if receipt_path else previous.get('previous_receipt'),
            'game': str(game), 'version': manifest['version'],
            'verified_files': len(manifest['files']), 'game_executable_unchanged': True,
            'settings_preserved': True, 'note': 'Roll back updates newest first. Do not remove user-modified files.'}
    deploy.atomic_write(latest_path, json.dumps(info, indent=2).encode())
    print('Verified installation. Existing calibration settings preserved. Launch through Steam; do not hot-unload.')
    return receipt_path

def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument('--game', type=Path, default=deploy.DEFAULT_GAME)
    result.add_argument('--build', type=Path, default=ROOT / 'build')
    result.add_argument('--dependency-dir', type=Path, default=Path.home() / 'Downloads')
    result.add_argument('--cc', help='Path to WinLibs gcc.exe')
    result.add_argument('--cxx', help='Path to WinLibs g++.exe')
    result.add_argument('--apply', action='store_true', help='Install after checks; otherwise only plan game changes')
    return result

if __name__ == '__main__':
    try: install(parser().parse_args())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))

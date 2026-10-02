"""Stage this build and hash-pinned local dependencies; never write to the game."""
from pathlib import Path
import argparse
import json
import re
import shutil
import tempfile
import zipfile

if __package__:
    from . import deploy
else:
    import deploy

ROOT = Path(__file__).resolve().parents[1]

def project_version() -> str:
    match = re.search(r'project\(ExplorationFirstPerson VERSION (\d+\.\d+\.\d+)', (ROOT / 'CMakeLists.txt').read_text())
    if not match: raise ValueError('Cannot determine project version')
    return match.group(1)

def dependency_archives(directory: Path | None = None) -> list[tuple[dict, Path]]:
    records = json.loads((ROOT / 'assets/dependencies.json').read_text())
    sources = []
    for record in records:
        source = deploy.contained(ROOT, record['archive'])
        if not source.is_file() and directory is not None:
            exact = directory / source.name
            candidates = [exact] if exact.is_file() else sorted(
                p for p in directory.glob('*.zip') if any(word in p.name.lower() for word in ('loader', 'modmenu')))
            source = next((p for p in candidates if deploy.digest(p) == record['sha256']), source)
        if not source.is_file():
            raise RuntimeError(f"Missing {record['name']} archive. See docs/INSTALLATION.md or use --dependency-dir.")
        if deploy.digest(source) != record['sha256']:
            raise RuntimeError(f'Archive hash changed: {source}')
        sources.append((record, source))
    return sources

def stage(build: Path = ROOT / 'build', dependency_dir: Path | None = None) -> Path:
    build = build.resolve()
    native = build / 'ExplorationFirstPerson.dll'
    if not native.is_file(): raise RuntimeError('Build the Windows x64 DLL before packaging')
    sources = dependency_archives(dependency_dir)  # validate before replacing any staged files
    target = deploy.contained(build, 'package')
    with tempfile.TemporaryDirectory(prefix='efp-stage-', dir=build) as temporary:
        staging = Path(temporary)
        own = staging / 'crmods/ExplorationFirstPerson'
        own.mkdir(parents=True)
        shutil.copyfile(native, own / native.name)
        for name in ('exploration_first_person.menu.json', 'ExplorationFirstPerson.ini'):
            shutil.copyfile(ROOT / 'assets' / name, own / name)
        shutil.copyfile(ROOT / 'third_party/minhook/LICENSE.txt', own / 'MinHook-LICENSE.txt')
        for record, source in sources:
            with zipfile.ZipFile(source) as archive:
                for item in archive.infolist():
                    if item.is_dir(): continue
                    if record['name'] == 'CRLoader-1.0.0' and item.filename != 'winmm.dll': continue
                    if record['name'] == 'CRModMenu-1.7.0' and not item.filename.startswith('crmods/CRModMenu/'): continue
                    dest = deploy.contained(staging, item.filename)
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes(archive.read(item))
        files = [{'path': p.relative_to(staging).as_posix(), 'sha256': deploy.digest(p)}
                 for p in sorted(staging.rglob('*')) if p.is_file()]
        manifest = {'schema': 1, 'version': project_version(), 'mode': 'experimental_camera_preview', 'files': files}
        (staging / 'package-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        if target.exists(): shutil.rmtree(target)  # generated staging only, never installed files
        shutil.copytree(staging, target)
    print(f'Staged {len(files)} verified files in {target}')
    return target

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--dependency-dir', type=Path)
    args = parser.parse_args()
    stage(args.build, args.dependency_dir)

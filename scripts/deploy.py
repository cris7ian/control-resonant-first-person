"""Reversible installation. Dry-run is the default; never executes or launches a DLL."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tempfile
import uuid
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GAME = Path(r"G:\SteamLibrary\steamapps\common\CONTROL Resonant")

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def relative_path(text: str) -> Path:
    p = PurePosixPath(text)
    if p.is_absolute() or not p.parts or any(x in ('..', '.') or ':' in x or '\\' in x for x in p.parts):
        raise ValueError(f"Unsafe relative path: {text}")
    return Path(*p.parts)

def contained(root: Path, relative: str) -> Path:
    target = root / relative_path(relative)
    if not target.resolve().is_relative_to(root.resolve()):
        raise ValueError(f"Path escapes installation root: {relative}")
    # Refuse Windows junctions/symlinks along the path, including the root.
    current = target
    while True:
        if current.exists() and (current.is_symlink() or (getattr(current.lstat(), 'st_file_attributes', 0) & 0x400)):
            raise ValueError(f"Refusing linked installation path: {current}")
        if current == root: break
        current = current.parent
    return target

def ensure_closed() -> None:
    if os.name != 'nt': return
    result = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq CONTROLResonant.exe', '/FO', 'CSV', '/NH'],
                            capture_output=True, text=True, check=True)
    if '"controlresonant.exe"' in result.stdout.lower():
        raise RuntimeError('Close CONTROL Resonant before installing or removing mods.')

def atomic_write(path: Path, content: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp = tempfile.mkstemp(prefix='.efp-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)
    finally:
        if os.path.exists(temp): os.unlink(temp)

def install_plan(package: Path, game: Path) -> list[dict]:
    manifest = json.loads((package / 'package-manifest.json').read_text(encoding='utf-8'))
    if manifest.get('schema') != 1 or not manifest.get('files'): raise ValueError('Invalid package manifest')
    changes, seen = [], set()
    for item in manifest['files']:
        rel = item['path']
        folded = rel.casefold()
        if folded in seen: raise ValueError(f'Duplicate destination: {rel}')
        seen.add(folded)
        if not (rel == 'winmm.dll' or rel.startswith('crmods/CRModMenu/') or rel.startswith('crmods/ExplorationFirstPerson/')):
            raise ValueError(f'Unexpected package destination: {rel}')
        source, dest = contained(package, rel), contained(game, rel)
        if not source.is_file() or digest(source) != item['sha256']:
            raise ValueError(f'Package hash mismatch: {rel}')
        before = digest(dest) if dest.is_file() else None
        if dest.exists() and not dest.is_file(): raise ValueError(f'Destination is not a file: {rel}')
        if before == item['sha256']: continue  # reused dependencies are not owned by this installation
        if before and not rel.startswith('crmods/ExplorationFirstPerson/'):
            raise RuntimeError(f'Existing dependency differs; reconcile it before installation: {dest}')
        changes.append({'path': rel, 'before_sha256': before, 'after_sha256': item['sha256'], 'source': str(source)})
    camera = contained(game, 'crmods/BetterCamera/BetterCamera.dll')
    if camera.is_file():
        order = contained(game, 'crmods.txt')
        existing = order.read_bytes() if order.is_file() else b''
        text = existing.decode('utf-8-sig')
        lines = []
        for line in text.splitlines():
            fields = line.strip().split(maxsplit=1)
            if len(fields) == 2 and fields[0] in ('0', '1') and fields[1].replace('/', '\\').casefold() == 'crmods\\bettercamera\\bettercamera.dll':
                continue
            lines.append(line)
        lines.append('0 crmods\\BetterCamera\\BetterCamera.dll')
        content = ('\r\n'.join(lines) + '\r\n').encode('utf-8')
        if content != existing:
            changes.append({'path': 'crmods.txt', 'before_sha256': digest(order) if order.is_file() else None,
                            'after_sha256': hashlib.sha256(content).hexdigest(), 'content_hex': content.hex()})
    return changes

def rollback(game: Path, receipt: dict, backups: Path, apply: bool) -> list[str]:
    conflicts, ready = [], []
    for item in reversed(receipt['changes']):
        rel = item['path']
        if not (rel in ('winmm.dll', 'crmods.txt') or rel.startswith('crmods/CRModMenu/') or rel.startswith('crmods/ExplorationFirstPerson/')):
            raise ValueError(f'Unexpected rollback destination: {rel}')
        if item.get('restored'): continue
        dest = contained(game, rel)
        actual = digest(dest) if dest.is_file() else None
        if dest.exists() and not dest.is_file():
            conflicts.append(rel); continue
        if actual == item['before_sha256']:
            if apply: item['restored'] = True
            continue  # intent was journaled but not applied, or already restored
        if actual is not None and actual != item['after_sha256']:
            conflicts.append(rel); continue
        data = None
        if item['before_sha256']:
            saved = contained(backups, rel)
            if not saved.is_file() or digest(saved) != item['before_sha256']:
                raise RuntimeError(f'Invalid/missing rollback backup: {rel}')
            data = saved.read_bytes()
        ready.append((item, dest, actual, data))
    # Validate every backup before the first rollback mutation.
    if apply:
        for item, dest, expected, data in ready:
            ensure_closed()
            if (digest(dest) if dest.is_file() else None) != expected:
                conflicts.append(item['path']); continue
            if data is not None: atomic_write(dest, data)
            elif dest.is_file(): dest.unlink()
            item['restored'] = True
    return conflicts

def perform_install(package: Path, game: Path, receipts: Path, apply: bool) -> Path | None:
    ensure_closed()
    if not (game / 'CONTROLResonant.exe').is_file(): raise RuntimeError('Game executable not found')
    if receipts.resolve().is_relative_to(game.resolve()): raise ValueError('Keep backups outside the game directory')
    changes = install_plan(package, game)
    print(('APPLY' if apply else 'DRY RUN') + f': {len(changes)} file changes; game executable SHA256 {digest(game / "CONTROLResonant.exe")}')
    for item in changes: print(('BACKUP + REPLACE ' if item['before_sha256'] else 'ADD ') + item['path'])
    if not apply or not changes: return None
    # Preflight and recheck all destinations before the first mutation.
    ensure_closed()
    for item in changes:
        dest = contained(game, item['path'])
        actual = digest(dest) if dest.is_file() else None
        if actual != item['before_sha256']: raise RuntimeError(f'Destination changed since dry run: {dest}')
        if 'source' in item and digest(Path(item['source'])) != item['after_sha256']:
            raise RuntimeError('Package changed during installation')
    directory = receipts / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ') + '-' + uuid.uuid4().hex[:8])
    directory.mkdir(parents=True, exist_ok=False)
    receipt = {'schema': 1, 'game': str(game.resolve()), 'status': 'in_progress',
               'game_sha256': digest(game / 'CONTROLResonant.exe'), 'changes': []}
    receipt_path = directory / 'receipt.json'
    atomic_write(receipt_path, json.dumps(receipt, indent=2).encode())
    try:
        for item in changes:
            ensure_closed()
            dest = contained(game, item['path'])
            if (digest(dest) if dest.is_file() else None) != item['before_sha256']:
                raise RuntimeError(f'Destination changed: {dest}')
            if item['before_sha256']:
                atomic_write(contained(directory / 'backups', item['path']), dest.read_bytes())
            data = Path(item['source']).read_bytes() if 'source' in item else bytes.fromhex(item['content_hex'])
            if hashlib.sha256(data).hexdigest() != item['after_sha256']: raise RuntimeError('Source changed')
            # Journal intent before changing the destination; recovery is safe if interrupted here.
            receipt['changes'].append({k: item[k] for k in ('path', 'before_sha256', 'after_sha256')})
            atomic_write(receipt_path, json.dumps(receipt, indent=2).encode())
            atomic_write(dest, data)
            if digest(dest) != item['after_sha256']: raise RuntimeError(f'Post-copy hash mismatch: {dest}')
        receipt['status'] = 'installed'
    except Exception:
        conflicts = rollback(game, receipt, directory / 'backups', True)
        receipt['status'] = 'rollback_conflicts' if conflicts else 'rolled_back'
        receipt['conflicts'] = conflicts
        atomic_write(receipt_path, json.dumps(receipt, indent=2).encode())
        raise
    atomic_write(receipt_path, json.dumps(receipt, indent=2).encode())
    print(f'Rollback receipt: {receipt_path}')
    return receipt_path

def uninstall(receipt_path: Path, apply: bool) -> list[str]:
    ensure_closed()
    receipt = json.loads(receipt_path.read_text(encoding='utf-8'))
    if receipt.get('schema') != 1 or receipt.get('status') not in ('installed', 'in_progress', 'rollback_conflicts'):
        raise ValueError('Receipt is not an active installation')
    game = Path(receipt['game'])
    print(('APPLY' if apply else 'DRY RUN') + f': rollback {len(receipt["changes"])} files')
    conflicts = rollback(game, receipt, receipt_path.parent / 'backups', apply)
    for name in conflicts: print(f'PRESERVED user-modified file: {name}')
    if apply:
        receipt['status'] = 'rollback_conflicts' if conflicts else 'removed'
        receipt['conflicts'] = conflicts
        atomic_write(receipt_path, json.dumps(receipt, indent=2).encode())
    return conflicts

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['install', 'uninstall'])
    parser.add_argument('--package', type=Path, default=ROOT / 'build/package')
    parser.add_argument('--game', type=Path, default=DEFAULT_GAME)
    parser.add_argument('--receipt', type=Path)
    parser.add_argument('--apply', action='store_true', help='Modify files; otherwise only show the plan')
    args = parser.parse_args()
    if args.action == 'install': perform_install(args.package, args.game, ROOT / 'installations', args.apply)
    elif not args.receipt: parser.error('uninstall requires --receipt')
    else:
        if uninstall(args.receipt, args.apply): raise SystemExit(2)

if __name__ == '__main__':
    try: main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))

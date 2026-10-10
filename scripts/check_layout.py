"""Read-only verification of pinned executable profiles and their native references."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
from dataclasses import dataclass

ROOT = Path(__file__).resolve().parents[1]


def build_layouts() -> dict[str, dict[str, int]]:
    """Read the same complete profile registry used by the native adapter."""
    source = (ROOT / 'src/game_layout.hpp').read_text(encoding='utf-8')
    hashes = dict(re.findall(r'constexpr char (\w+)\[\]\s*=\s*"([0-9a-f]{64})"', source))
    fields = re.findall(r'\b(\w+_rva)\b', re.search(r'struct BuildLayout\s*\{([^}]+)', source)[1])
    registry = re.search(r'supported_builds\s*=\s*\{([^}]+)', source)[1]
    result = {}
    for name in re.findall(r'\b\w+\b', registry):
        body = re.search(r'constexpr BuildLayout ' + re.escape(name) + r'\s*=\s*\{([^}]+)', source)[1]
        hash_name, addresses = body.split(',', 1)
        values = [int(value, 16) for value in re.findall(r'0x[0-9a-fA-F]+', addresses)]
        if len(values) != len(fields): raise ValueError(f'Incomplete native profile: {name}')
        fingerprint = hashes[hash_name.strip()]
        if fingerprint in result: raise ValueError('Duplicate executable fingerprint')
        result[fingerprint] = dict(zip(fields, values))
    if not result: raise ValueError('No native executable profiles')
    return result


def signatures() -> dict[str, bytes]:
    source = (ROOT / 'src/game_layout.hpp').read_text(encoding='utf-8')
    result = {}
    for name in ('camera_prologue', 'mirror_prefix', 'builder_prefix', 'setter_prefix'):
        body = re.search(r'\b' + name + r'\s*=\s*\{([^}]+)', source)[1]
        result[name] = bytes(int(value, 16) for value in re.findall(r'0x[0-9a-fA-F]+', body))
    return result


@dataclass(frozen=True)
class Section:
    name: str
    rva: int
    virtual_size: int
    raw: int
    raw_size: int
    flags: int


class Executable:
    def __init__(self, image: bytes):
        self.image = image
        if len(image) < 64 or image[:2] != b'MZ': raise ValueError('Expected a Windows x64 executable')
        pe = struct.unpack_from('<I', image, 0x3c)[0]
        if pe + 26 > len(image) or image[pe:pe + 4] != b'PE\0\0': raise ValueError('Invalid PE header')
        machine, count, _, _, _, optional, flags = struct.unpack_from('<HHIIIHH', image, pe + 4)
        if machine != 0x8664 or flags & 0x2000 or optional < 112 or pe + 24 + optional > len(image):
            raise ValueError('Expected an AMD64 executable')
        if struct.unpack_from('<H', image, pe + 24)[0] != 0x20b: raise ValueError('Expected PE32+')
        self.image_size = struct.unpack_from('<I', image, pe + 24 + 56)[0]
        table = pe + 24 + optional
        if not 0 < count <= 96 or table + count * 40 > len(image): raise ValueError('Invalid PE section table')
        self.sections = []
        for index in range(count):
            offset = table + index * 40
            name = image[offset:offset + 8].rstrip(b'\0').decode('ascii')
            size, rva, raw_size, raw = struct.unpack_from('<IIII', image, offset + 8)
            characteristics = struct.unpack_from('<I', image, offset + 36)[0]
            if raw + raw_size > len(image) or rva + size > self.image_size: raise ValueError('Invalid PE section bounds')
            if any(max(rva, s.rva) < min(rva + size, s.rva + s.virtual_size) for s in self.sections):
                raise ValueError('Overlapping PE sections')
            self.sections.append(Section(name, rva, size, raw, raw_size, characteristics))

    def section(self, rva: int, size: int = 1) -> Section:
        for section in self.sections:
            if section.rva <= rva and rva + size <= section.rva + section.virtual_size: return section
        raise ValueError(f'RVA outside mapped sections: {rva:#x}')

    def read(self, rva: int, size: int) -> bytes:
        section = self.section(rva, size)
        offset = rva - section.rva
        if offset + size > section.raw_size: raise ValueError(f'RVA has no file data: {rva:#x}')
        return self.image[section.raw + offset:section.raw + offset + size]

    def reference(self, rva: int, opcode: bytes, target: int) -> None:
        instruction = self.read(rva, len(opcode) + 4)
        if not instruction.startswith(opcode): raise ValueError(f'Reference opcode mismatch at {rva:#x}')
        actual = rva + len(instruction) + struct.unpack_from('<i', instruction, len(opcode))[0]
        if actual != target: raise ValueError(f'Reference mismatch at {rva:#x}: {actual:#x} != {target:#x}')


def validate_profile(executable: Executable, build: dict[str, int]) -> None:
    prefixes = signatures()
    hooks = {'camera_update_rva': 'camera_prologue', 'mirror_rva': 'mirror_prefix',
             'projection_builder_rva': 'builder_prefix', 'fov_setter_rva': 'setter_prefix'}
    pdata = next((s for s in executable.sections if s.name == '.pdata'), None)
    if pdata is None: raise ValueError('Missing native function table')
    entries = executable.read(pdata.rva, pdata.virtual_size)
    starts = {struct.unpack_from('<I', entries, offset)[0] for offset in range(0, len(entries) - 11, 12)}
    for field, name in hooks.items():
        rva, prefix = build[field], prefixes[name]
        if executable.section(rva, len(prefix)).flags & 0x20000000 == 0: raise ValueError(f'Non-executable hook: {field}')
        # The leaf setter has no unwind entry. Detour targets do have entries.
        if field != 'fov_setter_rva' and rva not in starts:
            raise ValueError(f'Hook is not a native function start: {field}')
        if executable.read(rva, len(prefix)) != prefix: raise ValueError(f'Signature mismatch: {field}')
    text = next((s for s in executable.sections if s.name == '.text'), None)
    if text is None: raise ValueError('Missing .text section')
    if executable.read(text.rva, text.virtual_size).count(prefixes['camera_prologue']) != 1:
        raise ValueError('Camera prologue is not unique')
    for field, size in (('mode_rva', 4), ('render_camera_rva', 0x2e0), ('override_flag_rva', 1)):
        flags = executable.section(build[field], size).flags
        if not flags & 0x80000000 or flags & 0x20000000: raise ValueError(f'Invalid writable data: {field}')
    flags = executable.section(build['render_vtable_rva'], 8).flags
    if flags & (0x80000000 | 0x20000000) or not flags & 0x40000000: raise ValueError('Invalid render vtable section')
    # Decode each RIP-relative dependency independently. Section-wide shifts are not reliable.
    mode = build['mode_setter_rva']
    if executable.read(mode, 6) != bytes.fromhex('40 53 48 83 ec 20') or \
            executable.read(mode + 13, 5) != bytes.fromhex('45 33 c0 8b d9'):
        raise ValueError('Camera mode setter signature mismatch')
    executable.reference(mode + 18, b'\x89\x0d', build['mode_rva'])
    builder, caller = build['projection_builder_rva'], build['projection_caller_rva']
    executable.reference(caller + 0x12c, b'\x4c\x8d\x3d', build['render_camera_rva'])
    executable.reference(caller + 0x2c3, b'\xe8', builder)
    executable.reference(builder + 0xf4, b'\x44\x38\x35', build['override_flag_rva'])
    executable.reference(builder + 0x235, b'\xe8', build['fov_setter_rva'])
    executable.reference(builder + 0x179, b'\xe8', build['lens_constructor_rva'])
    executable.reference(build['lens_constructor_rva'] + 0x251, b'\x48\x8d\x05', build['render_vtable_rva'])
    # The complete setter signature includes its branch to the native projection rebuild.
    rebuild = build['fov_setter_rva'] + 0xbd0
    executable.reference(build['fov_setter_rva'] + 10, b'\xe9', rebuild)
    if rebuild not in starts: raise ValueError('Native projection rebuild is not a function start')


def verify(path: Path) -> dict:
    image = path.read_bytes()
    fingerprint = hashlib.sha256(image).hexdigest()
    build = build_layouts().get(fingerprint)
    if build is None: raise RuntimeError(f'Unsupported executable SHA256: {fingerprint}. Do not bypass the native compatibility gate.')
    validate_profile(Executable(image), build)
    return {'sha256': fingerprint, 'size': len(image), 'static_layout_verified': True,
            'live_validation': 'not established by this check',
            'rvas': {name: hex(value) for name, value in build.items()}}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    args = parser.parse_args()
    try: print(json.dumps(verify(args.executable), indent=2))
    except (OSError, ValueError, RuntimeError, struct.error) as error: raise SystemExit(str(error))

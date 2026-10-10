"""Owned PE fixtures exercise layout guards; they do not establish live compatibility."""
from pathlib import Path
import hashlib
import struct
import tempfile
import unittest
from unittest.mock import patch

from scripts import check_layout, install


class LayoutFixture:
    def __init__(self):
        self.build = dict(zip(check_layout.build_layouts()[install.supported_hash()],
            (0x1100, 0x6000, 0x1200, 0x1400, 0x2000, 0x6100, 0x5000, 0x6400,
             0x1800, 0x2400, 0x3000)))
        self.image = bytearray(0x6600)
        self.image[:2] = b'MZ'
        struct.pack_into('<I', self.image, 0x3c, 0x80)
        self.image[0x80:0x84] = b'PE\0\0'
        struct.pack_into('<HHIIIHH', self.image, 0x84, 0x8664, 4, 0, 0, 0, 0xf0, 2)
        struct.pack_into('<H', self.image, 0x98, 0x20b)
        struct.pack_into('<I', self.image, 0x98 + 56, 0x8000)
        self.sections = [(b'.text', 0x1000, 0x4000, 0x400, 0x60000020),
                         (b'.rdata', 0x5000, 0x1000, 0x4400, 0x40000040),
                         (b'.data', 0x6000, 0x1000, 0x5400, 0xc0000040),
                         (b'.pdata', 0x7000, 48, 0x6400, 0x40000040)]
        for index, (name, rva, size, raw, flags) in enumerate(self.sections):
            offset = 0x188 + index * 40
            self.image[offset:offset + 8] = name.ljust(8, b'\0')
            struct.pack_into('<IIII', self.image, offset + 8, size, rva, size, raw)
            struct.pack_into('<I', self.image, offset + 36, flags)
        prefixes = check_layout.signatures()
        for field, name in [('camera_update_rva', 'camera_prologue'), ('mirror_rva', 'mirror_prefix'),
                            ('projection_builder_rva', 'builder_prefix'), ('fov_setter_rva', 'setter_prefix')]:
            self.put(self.build[field], prefixes[name])
        starts = [self.build[field] for field in ('camera_update_rva', 'mirror_rva', 'projection_builder_rva')]
        starts.append(self.build['fov_setter_rva'] + 0xbd0)
        for index, start in enumerate(starts): self.put(0x7000 + index * 12, struct.pack('<III', start, start + 64, 0x5100))
        mode, builder, caller = (self.build[field] for field in ('mode_setter_rva', 'projection_builder_rva', 'projection_caller_rva'))
        self.put(mode, bytes.fromhex('40 53 48 83 ec 20'))
        self.put(mode + 13, bytes.fromhex('45 33 c0 8b d9'))
        self.reference(mode + 18, b'\x89\x0d', self.build['mode_rva'])
        self.reference(caller + 0x12c, b'\x4c\x8d\x3d', self.build['render_camera_rva'])
        self.reference(caller + 0x2c3, b'\xe8', builder)
        self.reference(builder + 0xf4, b'\x44\x38\x35', self.build['override_flag_rva'])
        self.reference(builder + 0x235, b'\xe8', self.build['fov_setter_rva'])
        self.reference(builder + 0x179, b'\xe8', self.build['lens_constructor_rva'])
        self.reference(self.build['lens_constructor_rva'] + 0x251, b'\x48\x8d\x05', self.build['render_vtable_rva'])

    def put(self, rva, data):
        for _, base, size, raw, _ in self.sections:
            if base <= rva and rva + len(data) <= base + size:
                self.image[raw + rva - base:raw + rva - base + len(data)] = data
                return
        raise AssertionError('Fixture write outside sections')

    def reference(self, rva, opcode, target):
        self.put(rva, opcode + struct.pack('<i', target - rva - len(opcode) - 4))

    def validate(self):
        check_layout.validate_profile(check_layout.Executable(bytes(self.image)), self.build)


class LayoutTests(unittest.TestCase):
    def test_native_registry_preserves_previous_and_current_build(self):
        profiles = check_layout.build_layouts()
        self.assertEqual(len(profiles), 2)
        self.assertEqual(set(install.supported_hashes()), set(profiles))
        previous = profiles['4f6596b08bb5bc7fe4150cf5b9f71d7bae87eea02d66627c03a5e05ffe84ea62']
        updated = profiles['9f56611d767c9227a8a1b250c01acf9478b50933c65f86a1e044fcc53df67cb1']
        self.assertEqual(previous['camera_update_rva'], 0x207bf90)
        self.assertEqual(updated['camera_update_rva'], 0x207bfa0)
        self.assertEqual(updated['mode_rva'], 0x5c24088)
        self.assertEqual(updated['fov_setter_rva'], 0x32285c0)
        self.assertEqual(updated['render_camera_rva'], 0x5c23ac0)
        self.assertEqual(updated['render_vtable_rva'], 0x478b4b0)
        self.assertEqual(updated['override_flag_rva'], 0x5c237e8)

    def test_all_static_dependencies_pass(self):
        LayoutFixture().validate()

    def test_leaf_setter_does_not_require_unwind_metadata(self):
        fixture = LayoutFixture()
        fixture.validate()  # No .pdata entry exists for the leaf setter.

    def test_every_address_rejects_drift(self):
        for field in LayoutFixture().build:
            with self.subTest(field=field), self.assertRaises(ValueError):
                fixture = LayoutFixture()
                fixture.build[field] += 16
                fixture.validate()

    def test_every_hook_signature_rejects_changes(self):
        for field in ('camera_update_rva', 'mirror_rva', 'projection_builder_rva', 'fov_setter_rva'):
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'Signature mismatch'):
                fixture = LayoutFixture()
                fixture.put(fixture.build[field], b'\xcc')
                fixture.validate()

    def test_duplicate_camera_prologue_rejected(self):
        fixture = LayoutFixture()
        fixture.put(0x4000, check_layout.signatures()['camera_prologue'])
        with self.assertRaisesRegex(ValueError, 'not unique'): fixture.validate()

    def test_missing_native_function_boundary_rejected(self):
        fixture = LayoutFixture()
        fixture.put(0x7000, struct.pack('<I', fixture.build['camera_update_rva'] + 1))
        with self.assertRaisesRegex(ValueError, 'function start'): fixture.validate()

    def test_wrong_section_access_rejected(self):
        for index, flags in ((0, 0x40000020), (1, 0xc0000040), (2, 0x40000040), (2, 0xe0000040)):
            with self.subTest(section=index, flags=flags), self.assertRaises(ValueError):
                fixture = LayoutFixture()
                struct.pack_into('<I', fixture.image, 0x188 + index * 40 + 36, flags)
                fixture.validate()

    def test_bad_reference_opcode_rejected(self):
        fixture = LayoutFixture()
        fixture.put(fixture.build['projection_builder_rva'] + 0xf4, b'\x90')
        with self.assertRaisesRegex(ValueError, 'opcode mismatch'): fixture.validate()

    def test_invalid_pe_headers_rejected(self):
        fixture = LayoutFixture()
        for image in (b'', b'not an executable', bytes(fixture.image[:128])):
            with self.subTest(size=len(image)), self.assertRaises(ValueError): check_layout.Executable(image)
        struct.pack_into('<H', fixture.image, 0x84, 0x14c)
        with self.assertRaisesRegex(ValueError, 'AMD64'): check_layout.Executable(fixture.image)

    def test_pe_bounds_and_overlaps_rejected(self):
        for offset, value in ((0x188 + 16, 0xfffffff0), (0x188 + 20, 0xfffffff0), (0x188 + 40 + 12, 0x2000)):
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                fixture = LayoutFixture()
                struct.pack_into('<I', fixture.image, offset, value)
                check_layout.Executable(fixture.image)

    def test_verification_and_install_use_same_profile(self):
        fixture = LayoutFixture()
        fingerprint = hashlib.sha256(fixture.image).hexdigest()
        with tempfile.TemporaryDirectory() as directory, patch.object(install.deploy, 'ensure_closed'), \
                patch.object(check_layout, 'build_layouts', return_value={fingerprint: fixture.build}):
            game = Path(directory)
            executable = game / 'CONTROLResonant.exe'
            executable.write_bytes(fixture.image)
            self.assertEqual(check_layout.verify(executable)['sha256'], fingerprint)
            self.assertEqual(install.check_game(game), fingerprint)
            # A fingerprint alone cannot admit incorrect addresses.
            fixture.build['mode_rva'] += 4
            with self.assertRaisesRegex(ValueError, 'Reference mismatch'): install.check_game(game)

    def test_unknown_hash_still_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / 'CONTROLResonant.exe'
            executable.write_bytes(LayoutFixture().image)
            with self.assertRaisesRegex(RuntimeError, 'Unsupported executable'): check_layout.verify(executable)


if __name__ == '__main__': unittest.main()

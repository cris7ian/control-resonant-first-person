"""Fast release guards with modeled PE data; real GNU strip runs in release_artifact_integration.py."""
from pathlib import Path
import hashlib
import json
import struct
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from scripts import release


def fake_dll(version='0.3.0', machine=0x8664) -> bytes:
    image = bytearray(320)
    image[:2] = b'MZ'
    struct.pack_into('<I', image, 0x3c, 0x80)
    image[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', image, 0x84, machine, 1, 0, 0, 0, 2, 0x2000)
    struct.pack_into('<H', image, 0x98, 0x20b)
    section = 0x9a
    image[section:section + 8] = b'.rdata\0\0'
    struct.pack_into('<IIII', image, section + 8, 64, 0x1000, 64, 256)
    struct.pack_into('<I', image, section + 36, 0x40000040)
    marker = (version + '\0').encode()
    image[256:256 + len(marker)] = marker
    return bytes(image)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / 'build'
        self.build.mkdir()
        (self.build / 'ExplorationFirstPerson.dll').write_bytes(fake_dll())
        for name in release.DOCS:
            dest = self.root / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text('# Public documentation\n')
        (self.root / 'assets').mkdir()
        (self.root / 'assets/ExplorationFirstPerson.ini').write_text('[Safety]\ncamera_writes=1\n')
        (self.root / 'assets/exploration_first_person.menu.json').write_text(json.dumps({'version': '0.3.0-preview'}))
        (self.root / 'third_party/minhook').mkdir(parents=True)
        (self.root / 'third_party/minhook/LICENSE.txt').write_text('MinHook notice\n')
        (self.root / 'licenses').mkdir()
        for name in release.LICENSES:
            (self.root / 'licenses' / name).write_text('Runtime notice\n')
        for context in (patch.object(release, 'ROOT', self.root),
                        patch.object(release.package, 'project_version', return_value='0.3.0'),
                        patch.object(release.subprocess, 'run')):
            context.start()
            self.addCleanup(context.stop)

    def make(self):
        return release.make_release(self.build, self.build / 'releases', 'strip', '1234567')

    def test_own_files_only_and_hashed_manifest(self):
        # Local dependency-inclusive staging and calibration must never reach this archive.
        ignored = self.build / 'package/crmods/ExplorationFirstPerson/ModMenuConfig'
        ignored.mkdir(parents=True)
        (ignored / 'exploration_first_person.ini').write_text('personal calibration')
        (self.build / 'package/winmm.dll').write_bytes(b'private dependency')
        (self.build / 'package/steam_api64.dll').write_bytes(b'game library')
        archive_path, checksum = self.make()
        with zipfile.ZipFile(archive_path) as archive:
            self.assertIsNone(archive.testzip())
            names = set(archive.namelist())
            expected = set(release.DOCS) | {f'{release.MOD}/{name}' for name in
                ('ExplorationFirstPerson.dll', 'ExplorationFirstPerson.ini',
                 'exploration_first_person.menu.json', 'MinHook-LICENSE.txt', 'THIRD-PARTY-NOTICES.md')}
            expected |= {f'{release.MOD}/licenses/{name}' for name in release.LICENSES}
            self.assertEqual(names, expected | {'release-manifest.json'})
            manifest = json.loads(archive.read('release-manifest.json'))
            self.assertEqual(manifest['version'], '0.3.0')
            self.assertEqual(manifest['source_commit'], '1234567')
            self.assertEqual({item['path'] for item in manifest['files']}, expected)
            for item in manifest['files']:
                data = archive.read(item['path'])
                self.assertEqual(hashlib.sha256(data).hexdigest(), item['sha256'])
                self.assertEqual(len(data), item['size'])
        self.assertEqual(checksum.read_text().split()[0], hashlib.sha256(archive_path.read_bytes()).hexdigest())

    def test_non_x64_binary_rejected(self):
        (self.build / 'ExplorationFirstPerson.dll').write_bytes(fake_dll(machine=0x14c))
        with self.assertRaisesRegex(ValueError, 'AMD64'):
            self.make()

    def test_stale_binary_rejected(self):
        (self.build / 'ExplorationFirstPerson.dll').write_bytes(fake_dll(version='0.2.4'))
        with self.assertRaisesRegex(ValueError, 'DLL version'):
            self.make()

    def test_descriptor_version_rejected(self):
        (self.root / 'assets/exploration_first_person.menu.json').write_text(json.dumps({'version': '0.2.4'}))
        with self.assertRaisesRegex(ValueError, 'Menu version'):
            self.make()

    def test_utf8_menu_descriptor_preserved(self):
        descriptor = json.dumps({'version': '0.3.0-preview', 'name': 'Camera ┐'}, ensure_ascii=False)
        (self.root / 'assets/exploration_first_person.menu.json').write_text(descriptor, encoding='utf-8')
        artifact, _ = self.make()
        with zipfile.ZipFile(artifact) as archive:
            self.assertEqual(archive.read(f'{release.MOD}/exploration_first_person.menu.json'), descriptor.encode('utf-8'))

    def test_stripped_runtime_changes_rejected(self):
        original = (self.build / 'ExplorationFirstPerson.dll').read_bytes()
        def corrupt(command, **kwargs):
            dll = Path(command[-1])
            data = bytearray(dll.read_bytes())
            data[319] ^= 1
            dll.write_bytes(data)
        release.subprocess.run.side_effect = corrupt
        with self.assertRaisesRegex(RuntimeError, 'runtime sections'):
            self.make()
        self.assertFalse(list((self.build / 'releases').glob('*.zip')))
        self.assertEqual((self.build / 'ExplorationFirstPerson.dll').read_bytes(), original)

    def test_malformed_pe_rejected(self):
        for image in (b'', b'not a DLL', fake_dll()[:180]):
            with self.subTest(size=len(image)), self.assertRaises(ValueError):
                release.runtime_sections(image)

    def test_dirty_git_tree_rejected(self):
        release.subprocess.run.return_value.stdout = ' M README.md\n'
        with self.assertRaisesRegex(RuntimeError, 'Commit the intended changes'):
            release.source_commit()

    def test_clean_git_revision_recorded(self):
        release.subprocess.run.return_value.stdout = ''
        with patch.object(release.subprocess, 'check_output', return_value='abcdef123\n'):
            self.assertEqual(release.source_commit(), 'abcdef123')

    def test_missing_notice_prevents_release(self):
        (self.root / 'licenses/winpthreads.txt').unlink()
        with self.assertRaises(FileNotFoundError):
            self.make()
        self.assertFalse(list((self.build / 'releases').glob('*.zip')))


if __name__ == '__main__':
    unittest.main()

"""Explicit Windows release check using the built DLL and real GNU strip; never installs files."""
from pathlib import Path
import argparse
import hashlib
import io
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import release


def pe_timestamp(image: bytes) -> int:
    pe = struct.unpack_from('<I', image, 0x3c)[0]
    return struct.unpack_from('<I', image, pe + 8)[0]


def section_count(image: bytes) -> int:
    pe = struct.unpack_from('<I', image, 0x3c)[0]
    return struct.unpack_from('<H', image, pe + 6)[0]


class ReleaseArtifactIntegration(unittest.TestCase):
    build = ROOT / 'build'
    strip_tool = shutil.which('strip')

    def test_real_strip_preserves_runtime_and_reproducible_archive(self):
        built_dll = self.build / 'ExplorationFirstPerson.dll'
        smoke_host = self.build / 'efp_native_smoke.exe'
        self.assertTrue(built_dll.is_file(), 'Build the Windows x64 DLL with RelWithDebInfo first')
        self.assertTrue(smoke_host.is_file(), 'Build the native smoke host with BUILD_TESTING enabled')
        self.assertIsNotNone(self.strip_tool, 'GNU strip is required; use --strip-tool')
        version = subprocess.run([self.strip_tool, '--version'], check=True, capture_output=True, text=True)
        self.assertIn('GNU strip', version.stdout, 'This check requires GNU BFD strip, not a mock or LLVM strip')
        original = built_dll.read_bytes()
        # A historical timestamp makes omission of SOURCE_DATE_EPOCH fail even within one second.
        native = bytearray(original)
        pe = struct.unpack_from('<I', native, 0x3c)[0]
        struct.pack_into('<I', native, pe + 8, 1234567890)
        native = bytes(native)
        runtime = release.runtime_sections(native)
        revision = 'integration-test'  # Temporary archives are not public release artifacts.
        with tempfile.TemporaryDirectory(prefix='efp-real-release-') as directory:
            root = Path(directory)
            build = root / 'input'
            build.mkdir()
            input_dll = build / built_dll.name
            input_dll.write_bytes(native)
            first_second = int(time.time())
            first, checksum = release.make_release(build, root / 'first', self.strip_tool, revision)
            archive_bytes = first.read_bytes()
            checksum_bytes = checksum.read_bytes()
            self.assertTrue(input_dll.read_bytes() == native, 'Packaging changed its input DLL')
            time.sleep(1.1)
            self.assertNotEqual(int(time.time()), first_second, 'Repeat packaging must run in a different second')
            second, second_checksum = release.make_release(build, root / 'second', self.strip_tool, revision)
            self.assertTrue(second.read_bytes() == archive_bytes, 'Real stripped ZIPs are not reproducible')
            self.assertEqual(second_checksum.read_bytes(), checksum_bytes)
            self.assertEqual(checksum_bytes.decode().split()[0], hashlib.sha256(archive_bytes).hexdigest())
            self.assertTrue(input_dll.read_bytes() == native, 'Repeat packaging changed its input DLL')
            with zipfile.ZipFile(io.BytesIO(archive_bytes)) as archive:
                self.assertIsNone(archive.testzip())
                manifest = json.loads(archive.read('release-manifest.json'))
                self.assertEqual(manifest['source_commit'], revision)
                self.assertEqual(manifest['version'], release.package.project_version())
                self.assertEqual({item['path'] for item in manifest['files']} | {'release-manifest.json'},
                                 set(archive.namelist()))
                for item in manifest['files']:
                    data = archive.read(item['path'])
                    self.assertEqual(hashlib.sha256(data).hexdigest(), item['sha256'], item['path'])
                    self.assertEqual(len(data), item['size'], item['path'])
                stripped = archive.read(f'{release.MOD}/ExplorationFirstPerson.dll')
            self.assertTrue(release.runtime_sections(stripped) == runtime, 'Stripping changed runtime sections')
            self.assertEqual(pe_timestamp(stripped), pe_timestamp(native), 'Stripping changed the input PE timestamp')
            self.assertLess(len(stripped), len(native), 'Use RelWithDebInfo: stripping must actually remove data')
            self.assertLess(section_count(stripped), section_count(native), 'Debug sections were not removed')
            smoke = root / 'smoke'
            smoke.mkdir()
            stripped_dll = smoke / built_dll.name
            stripped_dll.write_bytes(stripped)
            subprocess.run([str(smoke_host), str(stripped_dll)], check=True, capture_output=True, text=True)
        self.assertTrue(built_dll.read_bytes() == original, 'Integration verification changed the real build')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--strip-tool', default=shutil.which('strip'))
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('Run this check with a Windows x64 build and native smoke host')
    ReleaseArtifactIntegration.build = args.build.resolve()
    ReleaseArtifactIntegration.strip_tool = shutil.which(args.strip_tool) if args.strip_tool else None
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ReleaseArtifactIntegration)
    result = unittest.TextTestRunner(verbosity=2, buffer=True).run(suite)
    raise SystemExit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()

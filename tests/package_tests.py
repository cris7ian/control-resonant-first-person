"""Focused packaging, default-drift, and installation-preflight checks; no game injection."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import tempfile
import unittest
from unittest.mock import patch
import zipfile
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import package, install

class RepositoryContractTests(unittest.TestCase):
    def test_menu_and_compiled_defaults_match(self):
        source = (ROOT / 'src/core.hpp').read_text()
        values = dict(re.findall(r'\b(?:bool|int|Millis|float) (\w+) = ([^;]+);', source))
        options = json.loads((ROOT / 'assets/exploration_first_person.menu.json').read_text())
        for option in options['options']:
            raw = values[option['id']].strip()
            value = raw == 'true' if raw in ('true', 'false') else float(raw.rstrip('f'))
            self.assertEqual(option['default'], value, option['id'])
        self.assertEqual(options['version'], package.project_version())
    def test_menu_slider_bounds(self):
        options = json.loads((ROOT / 'assets/exploration_first_person.menu.json').read_text())
        for option in options['options']:
            if option.get('type') != 'slider':
                continue
            opt_id = option['id']
            low, high, default, step = option['min'], option['max'], option['default'], option['step']
            self.assertLessEqual(low, default, opt_id)
            self.assertLessEqual(default, high, opt_id)
            self.assertGreater(step, 0, opt_id)
            # Individual slider limits must not exceed the engine's hard validation bounds
            if opt_id == 'prototype_distance':
                self.assertGreaterEqual(low, -7.0)
                self.assertLessEqual(high, -5.0)
            elif opt_id == 'eye_height':
                self.assertGreaterEqual(low, -0.5)
                self.assertLessEqual(high, 0.5)
            elif opt_id == 'eye_side':
                self.assertGreaterEqual(low, -1.0)
                self.assertLessEqual(high, 1.0)
            elif opt_id == 'eye_forward':
                self.assertGreaterEqual(low, -0.3)
                self.assertLessEqual(high, 0.3)
            elif opt_id == 'transition_ms':
                self.assertGreaterEqual(low, 0)
                self.assertLessEqual(high, 500)
            elif opt_id == 'first_person_fov':
                self.assertGreaterEqual(low, 60)
                self.assertLessEqual(high, 120)
            elif opt_id == 'double_tap_ms':
                self.assertGreaterEqual(low, 150)
                self.assertLessEqual(high, 800)
            elif opt_id == 'max_tap_ms':
                self.assertGreaterEqual(low, 50)
                self.assertLessEqual(high, 500)
            elif opt_id == 'min_gap_ms':
                self.assertGreaterEqual(low, 10)
                self.assertLessEqual(high, 100)
    def test_confirmed_release_has_no_experimental_menu_labels(self):
        menu = json.loads((ROOT / 'assets/exploration_first_person.menu.json').read_text())
        self.assertEqual(menu['name'], 'Exploration First Person')
        for option in menu['options']:
            text = option.get('description', '').lower()
            self.assertNotIn('experimental', text, option['id'])
            self.assertNotIn('awaits live retesting', text, option['id'])
        # Keep the old key so existing calibration is not lost when the menu is renamed.
        self.assertIn('prototype_distance', {option['id'] for option in menu['options']})
    def test_documented_zip_examples_match_project_version(self):
        version = package.project_version()
        expected = f'control-resonant-first-person-{version}-windows-x64.zip'
        for name in ('README.md', 'docs/INSTALLATION.md'):
            text = (ROOT / name).read_text(encoding='utf-8')
            examples = re.findall(r'control-resonant-first-person-[0-9.]+-windows-x64\.zip', text)
            self.assertTrue(examples, name)
            self.assertTrue(all(item == expected for item in examples), name)
    def test_install_is_dry_run_by_default(self):
        self.assertFalse(install.parser().parse_args([]).apply)
    def test_unsupported_game_blocks_preflight(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(install.deploy, 'ensure_closed'):
            game = Path(directory)
            (game / 'CONTROLResonant.exe').write_bytes(b'unsupported executable')
            with self.assertRaisesRegex(RuntimeError, 'Unsupported executable'): install.check_game(game)

class StagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.build = self.root / 'build'; self.build.mkdir()
        (self.build / 'ExplorationFirstPerson.dll').write_bytes(b'our built DLL')
        (self.root / 'assets').mkdir()
        for name in ('ExplorationFirstPerson.ini', 'exploration_first_person.menu.json'):
            shutil.copyfile(ROOT / 'assets' / name, self.root / 'assets' / name)
        (self.root / 'CMakeLists.txt').write_text('project(ExplorationFirstPerson VERSION 0.2.3 LANGUAGES C CXX)')
        notices = self.root / 'third_party/minhook'; notices.mkdir(parents=True)
        (notices / 'LICENSE.txt').write_text('test notice')
        records = []
        for name, member in [('CRLoader-1.0.0', 'winmm.dll'), ('CRModMenu-1.7.0', 'crmods/CRModMenu/modsui.dll')]:
            archive = self.root / (name + '.zip')
            with zipfile.ZipFile(archive, 'w') as stream: stream.writestr(member, name.encode())
            records.append({'name': name, 'archive': archive.name, 'sha256': hashlib.sha256(archive.read_bytes()).hexdigest()})
        (self.root / 'assets/dependencies.json').write_text(json.dumps(records))
        self.replace_root = patch.object(package, 'ROOT', self.root); self.replace_root.start()
    def tearDown(self):
        self.replace_root.stop(); self.temp.cleanup()
    def test_stage_refreshes_assets_and_removes_stale_generated_files(self):
        target = package.stage(self.build)
        manifest = json.loads((target / 'package-manifest.json').read_text())
        self.assertEqual(manifest['version'], '0.2.3')
        self.assertEqual(manifest['mode'], 'anchored_exploration_camera')
        (target / 'crmods/ExplorationFirstPerson/stale.runtime.js').write_text('must not ship')
        asset = self.root / 'assets/exploration_first_person.menu.json'
        descriptor = json.loads(asset.read_text()); descriptor['name'] = 'changed without rebuilding DLL'
        asset.write_text(json.dumps(descriptor))
        package.stage(self.build)
        self.assertFalse((target / 'crmods/ExplorationFirstPerson/stale.runtime.js').exists())
        self.assertEqual(json.loads((target / 'crmods/ExplorationFirstPerson' / asset.name).read_text())['name'], descriptor['name'])
    def test_corrupt_archive_blocks_before_replacing_staged_package(self):
        target = package.stage(self.build)
        old = (target / 'package-manifest.json').read_bytes()
        (self.root / 'CRLoader-1.0.0.zip').write_bytes(b'corrupt archive')
        with self.assertRaisesRegex(RuntimeError, 'Archive hash changed'): package.stage(self.build)
        self.assertEqual((target / 'package-manifest.json').read_bytes(), old)
    def test_install_preserves_diagnostic_safety_setting(self):
        target = package.stage(self.build)
        game = self.root / 'game'
        rel = 'crmods/ExplorationFirstPerson/ExplorationFirstPerson.ini'
        safety = game / rel; safety.parent.mkdir(parents=True)
        safety.write_text('[Safety]\ncamera_writes=0\n')
        install.preserve_safety(target, game)
        self.assertEqual((target / rel).read_bytes(), safety.read_bytes())
        manifest = json.loads((target / 'package-manifest.json').read_text())
        entry = next(item for item in manifest['files'] if item['path'] == rel)
        self.assertEqual(entry['sha256'], hashlib.sha256(safety.read_bytes()).hexdigest())

    def test_stable_install_receipt_version_and_settings_preserved(self):
        staging = package.stage(self.build)
        game = self.root / 'game'
        game.mkdir()
        (game / 'CONTROLResonant.exe').write_bytes(b'test executable')
        config = game / 'crmods/ExplorationFirstPerson/ModMenuConfig/exploration_first_person.ini'
        config.parent.mkdir(parents=True)
        config.write_text('[Settings]\nprototype_distance=-6.35\ndebug=0\n')
        safety = config.parent.parent / 'ExplorationFirstPerson.ini'
        safety.write_text('[Safety]\ncamera_writes=0\n')
        before = (config.read_bytes(), safety.read_bytes())
        args = install.parser().parse_args(['--game', str(game), '--build', str(self.build), '--apply'])
        with patch.object(install, 'ROOT', self.root), patch.object(install, 'run'), \
                patch.object(install, 'check_game', return_value=install.deploy.digest(game / 'CONTROLResonant.exe')), \
                patch.object(install.package, 'stage', return_value=staging), \
                patch.object(install.deploy, 'ensure_closed'):
            receipt = install.install(args)
        latest = json.loads((self.root / 'analysis/latest-installation.json').read_text())
        self.assertEqual(latest['version'], '0.2.3')
        self.assertEqual(json.loads(receipt.read_text())['version'], '0.2.3')
        self.assertEqual((config.read_bytes(), safety.read_bytes()), before)
        self.assertTrue(latest['settings_preserved'])

if __name__ == '__main__': unittest.main()

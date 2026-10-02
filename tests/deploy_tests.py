import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

MODULE = Path(__file__).resolve().parents[1] / 'scripts/deploy.py'
spec = importlib.util.spec_from_file_location('deploy', MODULE)
deploy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deploy)

class DeploymentTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.game = self.root / 'game'
        self.package = self.root / 'package'
        self.receipts = self.root / 'receipts'
        self.game.mkdir(); self.package.mkdir()
        (self.game / 'CONTROLResonant.exe').write_bytes(b'test game unchanged')
        self.files = {
            'winmm.dll': b'loader',
            'crmods/CRModMenu/modsui.dll': b'menu',
            'crmods/ExplorationFirstPerson/ExplorationFirstPerson.dll': b'our mod',
        }
        self.make_package()
        self.closed = patch.object(deploy, 'ensure_closed')
        self.closed.start()
    def tearDown(self):
        self.closed.stop(); self.temp.cleanup()
    def put(self, rel, data):
        path = self.game / rel
        path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
        return path
    def make_package(self):
        items = []
        for rel, data in self.files.items():
            path = self.package / rel
            path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
            items.append({'path': rel, 'sha256': hashlib.sha256(data).hexdigest()})
        (self.package / 'package-manifest.json').write_text(json.dumps({'schema': 1, 'files': items}))
    def install(self, apply=True):
        return deploy.perform_install(self.package, self.game, self.receipts, apply)
    def test_dry_run_does_not_write(self):
        before = list(self.game.rglob('*'))
        self.assertIsNone(self.install(False))
        self.assertEqual(before, list(self.game.rglob('*')))
        self.assertFalse(self.receipts.exists())
    def test_install_and_remove(self):
        receipt = self.install()
        for rel, content in self.files.items(): self.assertEqual((self.game / rel).read_bytes(), content)
        self.assertEqual([], deploy.uninstall(receipt, True))
        for rel in self.files: self.assertFalse((self.game / rel).exists())
        self.assertEqual((self.game / 'CONTROLResonant.exe').read_bytes(), b'test game unchanged')
    def test_existing_identical_dependency_is_not_removed(self):
        self.put('winmm.dll', b'loader')
        receipt = self.install()
        deploy.uninstall(receipt, True)
        self.assertEqual((self.game / 'winmm.dll').read_bytes(), b'loader')
    def test_conflicting_dependency_blocks_before_writes(self):
        self.put('winmm.dll', b'another loader')
        with self.assertRaises(RuntimeError): self.install()
        self.assertFalse((self.game / 'crmods').exists())
    def test_corrupt_package_blocks_before_writes(self):
        (self.package / 'winmm.dll').write_bytes(b'tampered')
        with self.assertRaises(ValueError): self.install()
        self.assertFalse((self.game / 'winmm.dll').exists())
    def test_path_traversal_and_unexpected_destinations(self):
        for path in ('../outside', 'C:/outside', 'crmods/../outside', 'crmods\\outside', '/outside'):
            with self.assertRaises(ValueError): deploy.relative_path(path)
        manifest = {'schema': 1, 'files': [{'path': 'steam_api64.dll', 'sha256': 'bad'}]}
        (self.package / 'package-manifest.json').write_text(json.dumps(manifest))
        with self.assertRaises(ValueError): self.install()
    def test_user_modified_file_is_preserved(self):
        receipt = self.install()
        rel = 'crmods/ExplorationFirstPerson/ExplorationFirstPerson.dll'
        self.put(rel, b'user changed')
        self.assertEqual([rel], deploy.uninstall(receipt, True))
        self.assertEqual((self.game / rel).read_bytes(), b'user changed')
    def test_bettercamera_disabled_and_order_restored(self):
        self.put('crmods/BetterCamera/BetterCamera.dll', b'camera')
        original = b'1 crmods\\BetterCamera\\BetterCamera.dll\r\n1 crmods\\Other.dll\r\n'
        self.put('crmods.txt', original)
        receipt = self.install()
        order = (self.game / 'crmods.txt').read_text()
        self.assertNotIn('1 crmods\\BetterCamera', order)
        self.assertIn('0 crmods\\BetterCamera\\BetterCamera.dll', order)
        self.assertEqual([], deploy.uninstall(receipt, True))
        self.assertEqual((self.game / 'crmods.txt').read_bytes(), original)
        self.assertEqual((self.game / 'crmods/BetterCamera/BetterCamera.dll').read_bytes(), b'camera')
    def test_our_previous_version_is_backed_up(self):
        rel = 'crmods/ExplorationFirstPerson/ExplorationFirstPerson.dll'
        self.put(rel, b'previous version')
        receipt = self.install(); deploy.uninstall(receipt, True)
        self.assertEqual((self.game / rel).read_bytes(), b'previous version')
    def test_uninstall_dry_run(self):
        receipt = self.install()
        deploy.uninstall(receipt, False)
        self.assertTrue((self.game / 'winmm.dll').is_file())
        self.assertEqual(json.loads(receipt.read_text())['status'], 'installed')
    def test_repeat_conflicted_removal_skips_already_restored_files(self):
        own = 'crmods/ExplorationFirstPerson/ExplorationFirstPerson.dll'
        self.put(own, b'previous version')
        receipt = self.install()
        self.put('crmods/CRModMenu/modsui.dll', b'user menu change')
        self.assertEqual(['crmods/CRModMenu/modsui.dll'], deploy.uninstall(receipt, True))
        self.assertEqual((self.game / own).read_bytes(), b'previous version')
        self.assertEqual(['crmods/CRModMenu/modsui.dll'], deploy.uninstall(receipt, True))
        self.assertEqual((self.game / own).read_bytes(), b'previous version')
    def test_missing_backup_blocks_removal_before_any_mutation(self):
        own = 'crmods/ExplorationFirstPerson/ExplorationFirstPerson.dll'
        self.put(own, b'previous version')
        receipt = self.install()
        (receipt.parent / 'backups' / own).unlink()
        with self.assertRaises(RuntimeError): deploy.uninstall(receipt, True)
        self.assertEqual((self.game / 'winmm.dll').read_bytes(), b'loader')
        self.assertEqual((self.game / own).read_bytes(), b'our mod')
    def test_failed_install_rolls_back(self):
        actual = deploy.atomic_write
        failed = False
        def fail_once(path, content):
            nonlocal failed
            if not failed and path == self.game / 'crmods/CRModMenu/modsui.dll':
                failed = True
                raise OSError('simulated disk write failure')
            return actual(path, content)
        with patch.object(deploy, 'atomic_write', fail_once):
            with self.assertRaises(OSError): self.install()
        self.assertFalse((self.game / 'winmm.dll').exists())
        self.assertFalse((self.game / 'crmods/CRModMenu/modsui.dll').exists())
        receipt = next(self.receipts.glob('*/receipt.json'))
        self.assertEqual(json.loads(receipt.read_text())['status'], 'rolled_back')

if __name__ == '__main__': unittest.main()

# SPDX-License-Identifier: MPL-2.0
"""Keep helper imports lean without splitting the shared failure identity."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import qa_core
import support


class CoreIsolation(unittest.TestCase):
    def test_support_exports_the_same_failure_and_primitives(self):
        for name in ('Failure', 'require', 'digest', 'write_json'):
            self.assertIs(getattr(support, name), getattr(qa_core, name))
        with self.assertRaises(support.Failure):
            qa_core.require(False, 'same-failure')
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / 'nested' / 'receipt.json'
            support.write_json(path, {'status': 'passed'})
            self.assertEqual(json.loads(path.read_text()), {'status': 'passed'})
            self.assertEqual(support.digest(path), hashlib.sha256(path.read_bytes()).hexdigest())

    def test_fresh_helper_import_does_not_load_http_archive_or_runner_modules(self):
        code = """import json,sys
import darwin_inventory
blocked=('support','urllib.request','tarfile','ssl','shutil','failure_excerpt')
print(json.dumps({'loaded':sorted(name for name in blocked if name in sys.modules)}))
"""
        value = subprocess.run([sys.executable, '-c', code], cwd=Path(__file__).parent,
                               capture_output=True, text=True, check=True, timeout=10)
        self.assertEqual(json.loads(value.stdout), {'loaded': []})
        self.assertEqual(value.stderr, '')


if __name__ == '__main__':
    unittest.main()

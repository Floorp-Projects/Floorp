# SPDX-License-Identifier: MPL-2.0
from pathlib import Path
import tempfile
import unittest
import zipfile
import artifact_assembly_recipe as a

WORKFLOW=Path(__file__).resolve().parents[3]/'.github/workflows/package.yml'
class AssemblyTests(unittest.TestCase):
    def test_official_recipe_remains_artifact_only(self):
        with tempfile.TemporaryDirectory() as d:
            rows=a.write(WORKFLOW,Path(d)/'recipe')
            scripts=dict(a.extract(WORKFLOW))
            self.assertEqual(len(rows),11)
            self.assertIn('--enable-artifact-builds',scripts['Mach configure'])
            self.assertIn('--enable-debug',scripts['Mach configure'])
            self.assertIn('export PROVENANCE_MODE=v2',scripts['Build'])
            self.assertIn('sha256sum --check --strict',scripts['Build'])
            self.assertNotIn('Native compile and install tests',scripts)
            self.assertIn('./mach package',scripts['Package noraneko'])
    def test_engine_or_xhtml_mismatch_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'primary';q=Path(d)/'assembled';p.mkdir();q.mkdir();(q/'browser').mkdir()
            for n in ['floorp','libxul.so']:
                (p/n).write_bytes(b'\x7fELFfixture');(q/n).write_bytes(b'\x7fELFfixture')
            x=b'<script src="chrome://noraneko-startup/content/chrome_root.js"/>'
            with zipfile.ZipFile(q/'browser/omni.ja','w') as z:z.writestr('chrome/browser/content/browser/browser.xhtml',x)
            result=a.verify_product(p,q,x)
            self.assertEqual(result['chrome_registration'],'UNVERIFIED')
            with self.assertRaises(ValueError):a.verify_product(p,q,x+b'bad')
            (q/'libxul.so').write_bytes(b'\x7fELFwrong')
            with self.assertRaises(ValueError):a.verify_product(p,q,x)
if __name__=='__main__':unittest.main()

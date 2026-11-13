# SPDX-License-Identifier: MPL-2.0
import json
from pathlib import Path
import tempfile
import zipfile
import unittest
import yaml
import build_recipe as r
import build_worker as w

class RecipeTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.steps=[{'name':n,'run':'echo phase\n'} for n in r.PHASES]
        next(s for s in self.steps if s['name']=='Mach configure')['run']='echo begin\ncat >> mozconfig <<\'EOF\'\nac_add_options --disable-tests\nac_add_options --enable-artifact-builds\nEOF\n'
    def tearDown(self):self.tmp.cleanup()
    def workflow(self):
        path=self.root/'package.yml';path.write_text(yaml.safe_dump({'jobs':{'build':{'steps':self.steps}}}));return path
    def test_normal_native_sequence(self):
        steps=r.extract(self.workflow());names=[n for n,s in steps]
        self.assertLess(names.index('Apply Floorp common Runtime patches'),names.index('Native compile and install tests'))
        self.assertLess(names.index('Inject Noraneko'),names.index('Apply Firefox Preferences Override'))
        self.assertLess(names.index('Apply Firefox Preferences Override'),names.index('Native final package'))
        config=dict(steps)['Mach configure'];self.assertIn('--enable-tests',config);self.assertIn('--enable-debug',config)
        self.assertNotIn('--enable-artifact-builds',config);self.assertNotIn('--disable-tests',config)
    def test_missing_phase(self):
        self.steps.pop()
        with self.assertRaises(ValueError):r.extract(self.workflow())
    def test_duplicate_phase(self):
        self.steps.append(self.steps[0])
        with self.assertRaises(ValueError):r.extract(self.workflow())
    def test_unknown_expression_rejected(self):
        self.steps[0]['run']='echo ${{ secrets.TOKEN }}'
        with self.assertRaises(ValueError):r.extract(self.workflow())
    def test_changed_native_boundary(self):
        next(s for s in self.steps if s['name']=='Mach configure')['run']='echo different'
        with self.assertRaises(ValueError):r.extract(self.workflow())
    def test_all_generated_shell_parses(self):
        out=self.root/'recipe';rows=r.write(self.workflow(),out)
        self.assertEqual(len(rows),12);self.assertTrue((out/'recipe.json').is_file())
    def test_final_package_keeps_native_checks(self):
        script=dict(r.extract(self.workflow()))['Native final package']
        self.assertIn('./mach package',script);self.assertNotIn('sed ',script)
    def test_no_sign_or_publish_phase(self):
        for name,script in r.extract(self.workflow()):
            self.assertNotIn('gh release',script);self.assertNotIn('git push',script)

class StageTests(unittest.TestCase):
    def test_requires_packaged_not_dist_bin(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'dist/bin').mkdir(parents=True)
            with self.assertRaises(ValueError):w.stage_product(root,root/'out')
    def test_packaged_xhtml_must_match_actual_injection(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);src=root/'obj/dist/floorp'
            for name in ('floorp','application.ini','omni.ja','browser/buildid2'):
                p=src/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('fixture')
            with zipfile.ZipFile(src/'browser/omni.ja','w') as z:z.writestr('chrome/browser/content/browser/browser.xhtml','wrong product')
            with self.assertRaisesRegex(ValueError,'differs from actual injection'):
                w.stage_product(root/'obj',root/'product',b'actual injected XHTML')
    def test_stages_product_and_exact_modules(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);src=root/'obj/dist/floorp'
            for name in ('floorp','application.ini','omni.ja','browser/omni.ja','browser/buildid2'):
                p=src/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('fixture')
            modules=root/'obj/_tests/modules';modules.mkdir(parents=True);(modules/'a.js').write_text('same build')
            result=w.stage_product(root/'obj',root/'product')
            self.assertEqual(result,root/'product/floorp');self.assertEqual((result.parent/'testing-common/a.js').read_text(),'same build')

if __name__=='__main__':unittest.main()

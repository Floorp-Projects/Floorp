# SPDX-License-Identifier: MPL-2.0
import json
from pathlib import Path
import tempfile
import subprocess
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
    def test_native_metadata_backend_precedes_install(self):
        script=dict(r.extract(self.workflow()))['Native compile and install tests']
        lines=script.splitlines()
        self.assertEqual(lines,['./mach configure','./mach build --jobs="$(nproc)"',
                                './mach build-backend -b TestManifest','./mach build install-tests'])
    def test_native_metadata_backend_failure_stops_install(self):
        script=dict(r.extract(self.workflow()))['Native compile and install tests']
        mach=self.root/'mach'
        mach.write_text('#!/usr/bin/env bash\necho "$*" >> calls\nif [ "$1" = build-backend ]; then exit 42; fi\n')
        mach.chmod(0o755)
        result=subprocess.run(['bash','-euo','pipefail','-c',script],cwd=self.root,capture_output=True)
        self.assertEqual(result.returncode,42)
        self.assertNotIn('build install-tests',(self.root/'calls').read_text())
    def test_native_metadata_backend_success_reaches_install(self):
        script=dict(r.extract(self.workflow()))['Native compile and install tests']
        mach=self.root/'mach'
        mach.write_text('#!/usr/bin/env bash\necho "$*" >> calls\n')
        mach.chmod(0o755)
        result=subprocess.run(['bash','-euo','pipefail','-c',script],cwd=self.root,capture_output=True)
        self.assertEqual(result.returncode,0)
        self.assertEqual((self.root/'calls').read_text().splitlines()[-2:],
                         ['build-backend -b TestManifest','build install-tests'])
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

class PhaseEnvironmentTests(unittest.TestCase):
    def test_config_is_not_exported_until_configure_succeeds(self):
        with tempfile.TemporaryDirectory() as d:
            runtime=Path(d);env={'CI':'true'}
            for name in ['Apply upstream patches','Native bootstrap','Setup Noraneko']:
                env=w.environment_after_phase(name,env,runtime)
                self.assertNotIn('MOZCONFIG',env)
            (runtime/'mozconfig').write_text('ac_add_options --enable-tests')
            configured=w.environment_after_phase('Mach configure',env,runtime)
            self.assertNotIn('MOZCONFIG',env)
            self.assertEqual(configured['MOZCONFIG'],str(runtime/'mozconfig'))
            for name in ['Build Noraneko','Native compile and install tests','Inject Noraneko','Native final package']:
                self.assertEqual(w.environment_after_phase(name,configured,runtime),configured)
    def test_shell_template_assignment_stays_unexported_then_root_is_pinned(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);script=root/'configure.sh';log=root/'log'
            script.write_text('set -eu\nMOZCONFIG=template\nbash -c \'test "${MOZCONFIG+x}" != x\'\nprintf configured > mozconfig\n')
            env={'PATH':'/usr/bin:/bin','CI':'true'}
            w.run(script,root,env,log)
            configured=w.environment_after_phase('Mach configure',env,root)
            script.write_text('set -eu\ntest "$MOZCONFIG" = "$PWD/mozconfig"\ntest "$(cat "$MOZCONFIG")" = configured\n')
            w.run(script,root,configured,log)
    def test_missing_root_config_fails_closed(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(ValueError,'did not produce root mozconfig'):
                w.environment_after_phase('Mach configure',{'CI':'true'},Path(d))

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

# SPDX-License-Identifier: MPL-2.0
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
import zipfile

HERE = Path(__file__).parent
spec = importlib.util.spec_from_file_location('native_runner', HERE / 'run.py')
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)

class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name).resolve()
        self.runtime = self.root/'runtime'; self.runtime.mkdir()
        self.obj = self.runtime/'obj'; self.obj.mkdir()
        (self.runtime/'mach').write_text('placeholder, never executed')
        (self.obj/'.mozconfig').write_text('mk_add_options MOZ_OBJDIR=' + str(self.obj))
        (self.obj/'.mozconfig.json').write_text(json.dumps({'topsrcdir':str(self.runtime),'topobjdir':str(self.obj)}))
        (self.obj/'config.status').write_text(f'topsrcdir = {str(self.runtime)!r}\n')
        (self.obj/'mozinfo.json').write_text(json.dumps({'tests_enabled':True,'artifact':False}))
        self.resources = self.root/'product'; self.resources.mkdir()
        self.binary = self.resources/'floorp'; self.binary.write_text('product'); self.binary.chmod(0o700)
        (self.resources/'browser').mkdir()
        (self.resources/'browser/buildid2').write_text('frontend-proof')
        with zipfile.ZipFile(self.resources/'omni.ja','w') as z:z.writestr('chrome/nora-content/content/index.js','test fixture')
        (self.obj/'dist/bin').mkdir(parents=True)
        ini='[App]\nName=Floorp\nVersion=1\nBuildID=20260101000000\nID=test\n'
        (self.resources/'application.ini').write_text(ini)
        (self.obj/'dist/bin/application.ini').write_text(ini)
        self.modules=self.resources/'testing-common'; self.modules.mkdir()
        (self.obj/'_tests/modules').mkdir(parents=True)
        for directory in [self.modules,self.obj/'_tests/modules']:(directory/'A.sys.mjs').write_text('same build')
        self.args=SimpleNamespace(runtime=self.runtime,objdir=self.obj,binary=self.binary,resources=self.resources,modules=self.modules,python=Path('/usr/bin/python3'))
        self.config=json.loads((HERE/'suites.json').read_text())
    def tearDown(self): self.temp.cleanup()
    def test_matching_product_and_modules(self):
        self.assertEqual(r.preflight(self.args,{'suites':[]})['identity']['Name'],'Floorp')
    def test_absolute_harness_selection_is_rejected(self):
        suite = dict(self.config['suites'][0], path=str(self.runtime/'browser'))
        with self.assertRaisesRegex(ValueError, 'source-relative'):
            r.preflight(self.args, {'suites':[suite]})

    def test_mismatched_module_fails(self):
        (self.modules/'A.sys.mjs').write_text('other build')
        with self.assertRaises(ValueError):r.preflight(self.args,{'suites':[]})
    def test_different_product_build_fails(self):
        p=self.resources/'application.ini';p.write_text(p.read_text().replace('20260101000000','20260102000000'))
        with self.assertRaises(ValueError):r.preflight(self.args,{'suites':[]})
    def test_foreign_runtime_fails(self):
        (self.obj/'config.status').write_text("topsrcdir='/not/this/source'\n")
        with self.assertRaises(ValueError):r.preflight(self.args,{'suites':[]})
    def test_artifact_build_fails(self):
        (self.obj/'mozinfo.json').write_text('{"tests_enabled":true,"artifact":true}')
        with self.assertRaises(ValueError):r.preflight(self.args,{'suites':[]})
    def test_lock_collision_and_cleanup(self):
        path=self.root/'lock'
        with r.Lock(path):
            with self.assertRaises(FileExistsError):
                with r.Lock(path):pass
        self.assertFalse(path.exists())
    def test_commands_use_real_product_and_native_harness(self):
        for suite in self.config['suites']:
            cmd=r.command(self.args,self.config,suite,self.root/'logs')
            self.assertIn(str(self.binary),cmd)
            self.assertIn('--log-raw',cmd)
            self.assertNotIn('--allow-nonlocal-connections',cmd)
            if suite['kind'] != 'xpcshell': self.assertFalse(Path(cmd[-1]).is_absolute())
            if suite['kind']=='xpcshell':
                self.assertIn('--app-binary',cmd)
                self.assertEqual(cmd[cmd.index('--xre-path')+1],str(self.resources))
                self.assertIn('--manifest',cmd)
                self.assertNotIn(suite['path'],cmd)
            if suite['kind']=='browser':self.assertIn('--testing-modules-dir',cmd)
            if suite['kind']=='marionette':
                self.assertTrue(cmd[-1].endswith('/manifest.toml'))
                self.assertNotIn('--app-arg=--remote-allow-system-access',cmd)
    def wrapper_fixture(self, extra=''):
        suite = next(s for s in self.config['suites'] if s['kind'] == 'xpcshell')
        for base in (self.runtime, self.obj/'_tests/xpcshell'):
            directory = base/suite['path']; directory.mkdir(parents=True)
            (directory/'xpcshell.toml').write_text('[DEFAULT]\nfirefox-appdir="browser"\nhead="head.js"\n["test_one.js"]\nskip-if=["os == \'win\'"]\n' + extra)
        return suite

    def test_wrapper_plan_writes_nothing_and_includes_installed_original(self):
        suite = self.wrapper_fixture()
        logs = self.root/'not-created'
        spec = r.xpcshell_wrapper(self.args, suite, logs)
        self.assertFalse(logs.exists())
        parsed = r.tomllib.loads(spec['text'])
        self.assertEqual(parsed['DEFAULT'], {'floorp-appdir':'browser'})
        self.assertIn('include:' + spec['original'], parsed)
        self.assertTrue(spec['original'].startswith(str(self.obj/'_tests/xpcshell')))

    def test_wrapper_rejects_per_test_override(self):
        suite = self.wrapper_fixture('firefox-appdir="other"\n')
        with self.assertRaisesRegex(ValueError, 'Differing appdir'):
            r.xpcshell_wrapper(self.args, suite, self.root/'logs')

    def test_wrapper_rejects_nested_include(self):
        suite = self.wrapper_fixture('["include:other.toml"]\n')
        with self.assertRaisesRegex(ValueError, 'Nested includes'):
            r.xpcshell_wrapper(self.args, suite, self.root/'logs')


    def test_sessionstore_browser_manifests_are_separate(self):
        paths=[s['path'] for s in self.config['suites'] if s['priority']=='P1' and s['kind']=='browser']
        self.assertEqual(len(paths),3)
        self.assertTrue(all(p.endswith('.toml') for p in paths))
        self.assertIn('browser/components/sessionstore/test/browser_startup_telemetry.toml',paths)
    def test_all_requested_kinds_included(self):
        suites=self.config['suites']
        self.assertEqual({s['kind'] for s in suites if s['priority']=='P1'},{'browser','xpcshell','marionette'})
        self.assertEqual({s['kind'] for s in suites if s['priority']=='P2'},{'browser','xpcshell'})
    def test_p0_chunks_preserve_every_manifest_entry_and_native_file_selection(self):
        base = self.runtime/'browser/components/tabbrowser/test/browser/tabs'
        base.mkdir(parents=True)
        manifest = '[DEFAULT]\nprefs=["test.pref=true"]\n'
        for n in range(23):
            name = f'browser_test_{n}.js'
            (base/name).write_text('// fixture')
            manifest += f'\n["{name}"]\nskip-if=["true"]\n'
        (base/'browser.toml').write_text(manifest)
        config = r.chunk_p0({'suites':[self.config['suites'][0]], 'browser_preferences':{}}, self.runtime, 10)
        self.assertEqual([s['registered_files'] for s in config['suites']], [10, 10, 3])
        selected = [p for s in config['suites'] for p in s['test_paths']]
        self.assertEqual(len(set(selected)), 23)
        self.assertEqual((base/'browser.toml').read_text(), manifest)
        r.preflight(self.args, config)
        suite = config['suites'][0]
        cmd = r.command(self.args, config, suite, self.root/'logs')
        self.assertEqual(cmd[-10:], suite['test_paths'])
        self.assertNotIn('--timeout', cmd)
        suite['test_paths'][0] = 'browser/foreign.js'
        with self.assertRaises(ValueError): r.preflight(self.args, config)

    def test_p0_chunks_reject_invalid_sizes_or_manifest_entries(self):
        with self.assertRaises(ValueError): r.chunk_p0(self.config, self.runtime, 0)
        base = self.runtime/self.config['suites'][0]['path']
        base.mkdir(parents=True)
        (base/'browser.toml').write_text('["include:other.toml"]\n')
        with self.assertRaises(ValueError): r.chunk_p0(self.config, self.runtime, 10)

    def test_no_suite_end_does_not_claim_completion(self):
        path=self.root/'raw.jsonl';path.write_text('{"action":"test_end","status":"FAIL","expected":"PASS"}\n')
        result=r.structured_summary(path)
        self.assertFalse(result['suite_end']);self.assertEqual(result['unexpected_statuses'],1)


class CompletionTests(unittest.TestCase):
    def test_missing_suite_end_fails_even_exit_zero(self):
        self.assertFalse(r.passed(0,{'suite_end':False,'unexpected_statuses':0,'observed_unique_test_end':1,'executed_unique_test_end':1}))
    def test_unexpected_or_no_tests_fails(self):
        self.assertFalse(r.passed(0,{'suite_end':True,'unexpected_statuses':1,'observed_unique_test_end':1,'executed_unique_test_end':1}))
        self.assertFalse(r.passed(0,{'suite_end':True,'unexpected_statuses':0,'observed_unique_test_end':0,'executed_unique_test_end':0}))
    def test_only_complete_clean_exit_passes(self):
        s={'suite_end':True,'unexpected_statuses':0,'observed_unique_test_end':1,'executed_unique_test_end':1}
        self.assertTrue(r.passed(0,s));self.assertFalse(r.passed(1,s))
    def test_all_skipped_fails_but_expected_failure_counts_as_executed(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td)/'raw.jsonl'
            events = [{'action':'test_end','test':'skipped','status':'SKIP'},
                      {'action':'suite_end'}]
            path.write_text('\n'.join(json.dumps(e) for e in events))
            result = r.structured_summary(path)
            self.assertEqual(result['observed_unique_test_end'], 1)
            self.assertEqual(result['executed_unique_test_end'], 0)
            self.assertFalse(r.passed(0, result))
            # mozlog omits expected when an expected failure matches its outcome.
            events.insert(0, {'action':'test_end','test':'known-failure','status':'FAIL'})
            path.write_text('\n'.join(json.dumps(e) for e in events))
            result = r.structured_summary(path)
            self.assertEqual(result['executed_unique_test_end'], 1)
            self.assertTrue(r.passed(0, result))

    def test_retained_lock_requires_manual_cleanup(self):
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/'lock'
            with r.Lock(path) as lock:lock.retain=True
            self.assertTrue(path.exists())


class DryRunTests(unittest.TestCase):
    setUp = RunnerTests.setUp
    tearDown = RunnerTests.tearDown
    def test_dry_run_never_executes_or_locks(self):
        import contextlib
        import io
        from unittest.mock import patch
        args=[]
        for key in ('runtime','objdir','binary','resources','modules','python'):
            args += ['--'+key,str(getattr(self.args,key))]
        logs=self.root/'not-created';lock=self.root/'not-locked'
        args += ['--logs',str(logs),'--lock',str(lock),'--dry-run']
        with patch.object(r.subprocess,'run',return_value=SimpleNamespace(returncode=0)), patch.object(r,'xpcshell_wrapper',return_value={'planned': True}), patch.object(r,'preflight',return_value={'identity':{'Name':'Floorp'}}), patch.object(r,'execute') as execute, contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(r.main(args),0)
            execute.assert_not_called()
        self.assertFalse(logs.exists());self.assertFalse(lock.exists())

    def test_non_keyboard_launch_failure_retains_locks(self):
        from unittest.mock import patch
        args=[]
        for key in ('runtime','objdir','binary','resources','modules','python'):
            args += ['--'+key,str(getattr(self.args,key))]
        logs=self.root/'logs';lock=self.root/'lock'
        args += ['--logs',str(logs),'--lock',str(lock),'--suite','P0']
        with patch.object(r,'preflight',return_value={}), patch.object(r,'product_processes',return_value=[]), patch.object(r,'execute',side_effect=RuntimeError('test failure')) as execute:
            with self.assertRaises(RuntimeError): r.main(args)
            self.assertEqual(execute.call_args.args[1], self.runtime)
            self.assertEqual(execute.call_args.args[3]['MOZCONFIG'], str(self.obj/'.mozconfig'))
        self.assertTrue(lock.exists())
        self.assertTrue(json.loads((logs/'run.json').read_text())['manual_cleanup_required'])
        self.assertFalse(json.loads((logs/'run.json').read_text())['results'][0]['passed'])

if __name__=='__main__':unittest.main()

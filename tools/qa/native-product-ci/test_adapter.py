# SPDX-License-Identifier: MPL-2.0
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import local_cohort as c

spec=importlib.util.spec_from_file_location('adapter',Path(__file__).with_name('ci-adapter.py'))
a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)

class AdapterTests(unittest.TestCase):
    def test_tokens_not_forwarded(self):
        env=a.clean_environment({'PATH':'/bin','HOME':'/tmp/isolated','GITHUB_TOKEN':'secret','GH_TOKEN':'secret','AWS_SECRET_ACCESS_KEY':'secret','ACTIONS_RUNTIME_TOKEN':'secret','GITHUB_OUTPUT':'/tmp/output','PYTHONPATH':'injected'})
        self.assertEqual(env,{'PATH':'/bin','HOME':'/tmp/isolated','MOZ_DISABLE_NONLOCAL_CONNECTIONS':'1'})
    def test_all_groups_no_dryrun_or_selection(self):
        paths={k:'/safe/'+k for k in ('runtime','objdir','binary','resources','modules','python','xpcshell_python')}
        cmd=a.command({'paths':paths},Path('/logs'))
        for flag in ['--dry-run','--suite','--p0-chunk-size']:self.assertNotIn(flag,cmd)
        self.assertIn('--xpcshell-python',cmd)
        self.assertEqual(cmd[cmd.index('--binary')+1],'/safe/binary')
    def test_actual_xpcshell_paths(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);log=root/'raw.jsonl'
            record={'paths':{'binary':str(root/'floorp'),'resources':str(root)}}
            argv=[str(root/'floorp'),'--xpcshell','-g',str(root),'-a',str(root/'browser')]
            log.write_text('\n'.join(json.dumps(e) for e in [{'action':'test_start','test':'a'}, {'action':'log','message':'a | full command: '+repr(argv)}, {'action':'test_end','test':'a','status':'PASS'}]))
            self.assertEqual(len(a.audit_xpcshell(log,record)),1)
            argv[-1]='/foreign/browser';log.write_text('\n'.join(json.dumps(e) for e in [{'action':'test_start','test':'a'}, {'action':'log','message':'a | full command: '+repr(argv)}, {'action':'test_end','test':'a','status':'PASS'}]))
            with self.assertRaises(ValueError):a.audit_xpcshell(log,record)
    def test_argv_count_cannot_substitute_for_test_binding(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);log=root/'raw';record={'paths':{'binary':str(root/'floorp'),'resources':str(root)}}
            argv=[str(root/'floorp'),'--xpcshell','-g',str(root),'-a',str(root/'browser')]
            events=[{'action':'test_start','test':'a'},{'action':'test_start','test':'b'},
                    {'action':'log','message':'a | full command: '+repr(argv)},
                    {'action':'log','message':'a | full command: '+repr(argv)},
                    {'action':'test_end','test':'a','status':'PASS'},
                    {'action':'test_end','test':'b','status':'PASS'}]
            log.write_text('\n'.join(json.dumps(e) for e in events))
            with self.assertRaises(ValueError):a.audit_xpcshell(log,record)
    def test_argv_for_inactive_test_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);log=root/'raw';record={'paths':{'binary':str(root/'floorp'),'resources':str(root)}}
            argv=[str(root/'floorp'),'--xpcshell','-g',str(root),'-a',str(root/'browser')]
            log.write_text(json.dumps({'action':'log','message':'a | full command: '+repr(argv)}))
            with self.assertRaises(ValueError):a.audit_xpcshell(log,record)
    def test_missing_actual_argv_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            log=Path(d)/'raw';log.write_text('')
            with self.assertRaises(ValueError):a.audit_xpcshell(log,{'paths':{'binary':'/product/floorp','resources':'/product'}})
    def test_v3_loaded(self):
        r=c.load_runner()
        self.assertTrue(callable(r.prepare_xpcshell_wrapper))
        self.assertIn('--xre-path',c.RUNNER.read_text())
    def test_convert_keeps_missing_groups_missing(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);raw=root/'native';raw.mkdir()
            run={'plan':[{'id':g} for g in a.gate.GROUPS],'results':[], 'finished':'now'}
            converted=a.convert(run,{'schema':1},raw,root)
            self.assertEqual(converted['groups'],[])
    def test_subset_plan_rejected(self):
        with self.assertRaises(ValueError):a.convert({'plan':[]},{},Path('/tmp/a'),Path('/tmp'))
    def test_local_tree_digests_bytes_and_mode(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'file';p.write_text('first');before=c.tree(d);p.write_text('second')
            self.assertNotEqual(before,c.tree(d))
            before=c.tree(d);p.chmod(0o755);self.assertNotEqual(before,c.tree(d))
    def test_empty_tree_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(ValueError):c.tree(d)
    def test_broken_link_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d)/'bad').symlink_to('/nonexistent-native-ci')
            with self.assertRaises(ValueError):c.tree(d)
    def test_directory_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d)/'sub').mkdir();(Path(d)/'link').symlink_to(Path(d)/'sub')
            with self.assertRaises(ValueError):c.tree(d)
    def test_changed_snapshot_rejected(self):
        record={'schema':'floorp-local-native-cohort/1','paths':{}}
        with patch.object(c,'snapshot',return_value={'changed':True}):
            with self.assertRaises(ValueError):c.verify(record,'a'*40,'b'*40)
    def test_wrong_snapshot_schema(self):
        with self.assertRaises(ValueError):c.verify({},'a'*40,'b'*40)

if __name__=='__main__':unittest.main()

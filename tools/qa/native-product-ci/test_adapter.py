# SPDX-License-Identifier: MPL-2.0
import importlib.util
import json
import os
import shutil
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
        self.assertEqual(env,{'PATH':'/bin','HOME':'/tmp/isolated','MOZ_DISABLE_NONLOCAL_CONNECTIONS':'1','PYTHONDONTWRITEBYTECODE':'1'})
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
    def test_directory_symlink_inside_tree_is_digested_by_content(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)/'tree';(root/'sub').mkdir(parents=True);(root/'sub'/'f').write_text('x')
            (root/'link').symlink_to('sub')
            entries,links=c._walk(root)
            self.assertEqual(links,{'link':'sub'})
            self.assertEqual(entries['link/f'],entries['sub/f'])
            copy=Path(d)/'copy';shutil.copytree(root,copy,symlinks=False)
            self.assertEqual(c.tree(root),c.tree(copy))
    def test_directory_symlink_into_allowed_root_accepted(self):
        with tempfile.TemporaryDirectory() as d:
            src=Path(d)/'runtime'/'testing';src.mkdir(parents=True);(src/'t.js').write_text('t')
            obj=Path(d)/'runtime'/'obj'/'_tests';obj.mkdir(parents=True);(obj/'own').write_text('o')
            (obj/'testing').symlink_to(src)
            with self.assertRaises(ValueError):c.tree(obj)
            entries=c.tree(obj,(Path(d)/'runtime',))
            self.assertIn('testing/t.js',entries)
    def test_directory_symlink_leaving_trusted_roots_rejected(self):
        with tempfile.TemporaryDirectory() as d, tempfile.TemporaryDirectory() as outside:
            (Path(outside)/'secret').write_text('s');root=Path(d)/'tree';root.mkdir()
            (root/'own').write_text('o');(root/'escape').symlink_to(outside)
            with self.assertRaisesRegex(ValueError,'leaves trusted build roots: escape'):c.tree(root,(Path(d)/'other',))
    def test_directory_symlink_cycle_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'a').mkdir();(root/'a'/'f').write_text('x')
            (root/'a'/'loop').symlink_to(root)
            with self.assertRaisesRegex(ValueError,'cycle: a/loop'):c.tree(root)
    def test_indirect_directory_symlink_cycle_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)/'tree';root.mkdir();(root/'own').write_text('o')
            a=Path(d)/'a';b=Path(d)/'b';a.mkdir();b.mkdir()
            (a/'to_b').symlink_to(b);(b/'to_a').symlink_to(a);(root/'x').symlink_to(b)
            with self.assertRaisesRegex(ValueError,'cycle'):c.tree(root,(Path(d),))
    def test_broken_directory_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'own').write_text('o');(root/'gone').mkdir()
            (root/'link').symlink_to(root/'gone');(root/'gone').rmdir()
            with self.assertRaisesRegex(ValueError,'missing/special build material: link'):c.tree(root)
    def test_harness_bytecode_and_run_outputs_are_ephemeral(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'runtests.py').write_text('x');before=c.tree(root,ephemeral={'tests.json'})
            (root/'__pycache__').mkdir();(root/'__pycache__'/'runtests.cpython-312.pyc').write_bytes(b'b')
            (root/'stray.pyc').write_bytes(b'b');(root/'tests.json').write_text('{}')
            self.assertEqual(before,c.tree(root,ephemeral={'tests.json'}))
            # Only the named root-level file is ephemeral, not a nested one.
            (root/'sub').mkdir();(root/'sub'/'tests.json').write_text('{}')
            self.assertIn('sub/tests.json',c.tree(root,ephemeral={'tests.json'}))
            self.assertIn('tests.json',c.tree(root))
    def test_mochitest_tree_ephemeral_names_are_scoped(self):
        self.assertEqual(c.EPHEMERAL_TREE_FILES,{'_tests/testing/mochitest':frozenset({'tests.json','websock.log'})})
    def test_link_retarget_to_identical_content_is_detected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)/'tree';root.mkdir()
            for name in ('a','b'):(root/name).mkdir();(root/name/'f').write_text('same')
            (root/'link').symlink_to('a');(root/'file').symlink_to('a/f')
            entries,links=c._walk(root)
            (root/'link').unlink();(root/'link').symlink_to('b')
            (root/'file').unlink();(root/'file').symlink_to('b/f')
            entries2,links2=c._walk(root)
            self.assertEqual(entries,entries2)
            self.assertNotEqual(links,links2)
            self.assertEqual(links2,{'link':'b','file':'b/f'})
    def test_unreadable_directory_fails_closed(self):
        if os.geteuid()==0:self.skipTest('root can read any directory')
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'own').write_text('o');(root/'locked').mkdir();(root/'locked'/'f').write_text('x')
            (root/'locked').chmod(0)
            try:
                with self.assertRaises(OSError):c.tree(root)
            finally:(root/'locked').chmod(0o755)
    def test_differences_name_changed_paths(self):
        before={'trees':{'/o/t':{'a':{'sha256':'1'},'b':{'sha256':'2'}}},'directory_links':{'/o/t':{}},'files':{'x':'1'}}
        after={'trees':{'/o/t':{'a':{'sha256':'9'},'c':{'sha256':'3'}}},'directory_links':{'/o/t':{'l':'s'}},'files':{'x':'2'}}
        self.assertEqual(c.differences(before,after),
            ['directory_links:/o/t/l added','files changed','trees:/o/t/a changed','trees:/o/t/b removed','trees:/o/t/c added'])
        many={'trees':{'/t':{str(i):{} for i in range(30)}}}
        self.assertEqual(len(c.differences({'trees':{'/t':{}}},many)),21)
    def test_changed_snapshot_message_names_paths(self):
        record={'schema':'floorp-local-native-cohort/1','paths':{},'files':{'x':'1'}}
        with patch.object(c,'snapshot',return_value=dict(record,files={'x':'2'})):
            with self.assertRaisesRegex(ValueError,'files changed'):c.verify(record,'a'*40,'b'*40)
    def test_changed_snapshot_rejected(self):
        record={'schema':'floorp-local-native-cohort/1','paths':{}}
        with patch.object(c,'snapshot',return_value={'changed':True}):
            with self.assertRaises(ValueError):c.verify(record,'a'*40,'b'*40)
    def test_wrong_snapshot_schema(self):
        with self.assertRaises(ValueError):c.verify({},'a'*40,'b'*40)

if __name__=='__main__':unittest.main()

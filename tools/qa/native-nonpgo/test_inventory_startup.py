# SPDX-License-Identifier: MPL-2.0
"""Real interpreter startup controls; no native API success is simulated."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
import venv

import darwin_inventory as inventory
import helper_import_preflight as preflight
from process_census import BINDING_KEYS
from support import Runner


class InventoryStartupTests(unittest.TestCase):
    def test_private_cli_rejects_malformed_arguments_without_inventory_or_argument_echo(self):
        for arguments in ([],['--help'],['--pgid'],['--pgid','0'],['--pgid','-1'],
                ['--pgid','2147483648'],['--pgid','1.0'],['--pgid','PRIVATE'],
                ['--pgid','900','PRIVATE'],['--other','900']):
            with self.subTest(arguments=arguments):
                result=subprocess.run([sys.executable,'-S',str(Path(inventory.__file__)),*arguments],
                    capture_output=True,text=True,timeout=3)
                self.assertEqual(result.returncode,2)
                self.assertEqual(result.stderr,'')
                self.assertNotIn('PRIVATE',result.stdout)
                self.assertEqual(inventory.progress_reply(result.stdout)['events'],2)

    def test_fresh_inventory_import_graph_excludes_cli_parser_and_site(self):
        wrapper=('import json,os,runpy,sys;'
            'sys.path.insert(0,os.path.dirname(sys.argv[1]));'
            'sys.argv=[sys.argv[1],"--pgid","900"];'
            'runpy.run_path(sys.argv[0],run_name="__main__");'
            'sys.stderr.write(json.dumps({name:name in sys.modules for name in ("site","argparse")}))')
        result=subprocess.run([sys.executable,'-S','-c',wrapper,str(Path(inventory.__file__))],
            capture_output=True,text=True,timeout=3,env={'PYTHONDONTWRITEBYTECODE':'1'})
        self.assertEqual(result.returncode,0)
        self.assertEqual(json.loads(result.stderr),{'site':False,'argparse':False})
        decoded=inventory.decode_reply(result.stdout,900)
        self.assertEqual(decoded['status'],'unavailable');self.assertIsNone(decoded['pids'])

    def test_blocked_site_hook_is_reaped_and_no_site_inventory_still_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            environment=root/'python'
            venv.EnvBuilder(with_pip=False).create(environment)
            executable=environment/('Scripts/python.exe' if os.name=='nt' else 'bin/python')
            paths=subprocess.run([str(executable),'-c',
                'import json,site;print(json.dumps(site.getsitepackages()))'],
                check=True,capture_output=True,text=True,timeout=10)
            packages=next(Path(path) for path in json.loads(paths.stdout) if Path(path).is_dir())
            marker=root/'site-hook-entered'
            (packages/'blocked-inventory-startup.pth').write_text(
                'import pathlib,time;pathlib.Path('+repr(str(marker))+').write_text("entered");time.sleep(60)\n')
            child=subprocess.Popen([str(executable),'-c','pass'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            try:
                deadline=time.monotonic()+10
                while not marker.exists() and child.poll() is None and time.monotonic()<deadline:
                    time.sleep(0.01)
                self.assertTrue(marker.exists(),'control did not reach the installed site hook')
                with self.assertRaises(subprocess.TimeoutExpired):child.communicate(timeout=0.05)
            finally:
                if child.poll() is None:child.kill()
                child.communicate(timeout=5)
            self.assertIsNotNone(child.returncode)
            if os.name=='posix':
                with self.assertRaises(ChildProcessError):os.waitpid(child.pid,os.WNOHANG)
            marker.unlink()
            env={**{key:'startup-control' for key in BINDING_KEYS},'TASK_ROOT':str(root),
                 'FLOORP_CENSUS_ABI':str(root/'missing-abi.json'),'PYTHONDONTWRITEBYTECODE':'1'}
            original_run=subprocess.run
            observations=[]
            def invoke(argv,**kwargs):
                self.assertEqual(argv[0:2],[str(executable),'-S'])
                self.assertEqual(kwargs['timeout'],3)
                result=original_run(argv,**kwargs)
                decoded=inventory.decode_reply(result.stdout,900)
                self.assertEqual(decoded['status'],'unavailable')
                self.assertIsNone(decoded['pids'])
                self.assertGreaterEqual(inventory.progress_reply(result.stdout)['events'],2)
                observations.append(decoded)
                return result
            with mock.patch('support.sys.executable',str(executable)), \
                 mock.patch('support.sys.platform','darwin'),mock.patch.dict(os.environ,env,clear=True), \
                 mock.patch('support.subprocess.run',side_effect=invoke),mock.patch('support.time.sleep'):
                result=Runner.group_activity(900)
            self.assertEqual(len(observations),3)
            self.assertFalse(result['sampleComplete'])
            self.assertIsNone(result['rssMiB'])
            self.assertEqual(result['sampleAttempts'],3)
            self.assertFalse(marker.exists())

    def test_real_preflight_wrappers_use_no_site_for_both_import_graphs(self):
        env={**{key:'startup-control' for key in BINDING_KEYS},'TASK_ROOT':'/missing-task',
             'FLOORP_CENSUS_ABI':'/missing-task/census-abi.json'}
        original_run=subprocess.run
        observations=[]
        def invoke(argv,**kwargs):
            self.assertEqual(argv[1:3],['-S','-c'])
            self.assertEqual(kwargs['timeout'],3)
            result=original_run(argv,**kwargs)
            observations.append(preflight.import_graph(result.stderr,argv[4]))
            return result
        with mock.patch('helper_import_preflight.subprocess.run',side_effect=invoke):
            for variant in preflight.VARIANTS:
                result=preflight.sample(variant,'idle',1,env,os.getpgrp())
                self.assertEqual(result['status'],'inventory-unavailable')
                self.assertIsNone(result['measurement'])
                self.assertTrue(result['cleanup']['reaped'])
        self.assertEqual([row['modules']['support'] for row in observations],[False,True])


if __name__=='__main__':unittest.main()

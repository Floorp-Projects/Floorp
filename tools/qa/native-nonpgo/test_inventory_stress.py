# SPDX-License-Identifier: MPL-2.0
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest import mock

import inventory_stress as stress
from support import Failure


class InventoryStressTests(unittest.TestCase):
    def test_actual_start_barrier_blocks_until_release_and_eof_fails(self):
        for release in (True,False):
            with subprocess.Popen([sys.executable,str(Path(stress.__file__)),'--worker','--seconds','0.03',
                                   '--mib','1','--start-barrier'],stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) as child:
                try:
                    self.assertEqual(child.stdout.readline(),'ready\n')
                    time.sleep(0.08)
                    self.assertIsNone(child.poll())
                    if release:child.stdin.write('1');child.stdin.flush()
                    child.stdin.close()
                    self.assertEqual(child.wait(timeout=5)==0,release)
                finally:
                    if child.poll() is None:child.kill();child.wait(timeout=5)

    def test_actual_small_supervisor_releases_all_ready_workers_and_reaps_every_child(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'receipt.json'
            with mock.patch.object(stress,'SECONDS',0.03),mock.patch.object(stress,'WORKER_MIB',1), \
                 mock.patch.object(stress,'READY_SECONDS',5),mock.patch('sys.stdout',new=io.StringIO()):
                stress.workload(path)
            value=json.loads(path.read_text())
        self.assertEqual(value['status'],'passed');self.assertEqual(value['readyWorkers'],3)
        self.assertEqual(value['barriersReleased'],3);self.assertGreaterEqual(value['loadSeconds'],0.03)
        self.assertEqual(value['phase'],'finished');self.assertIsNone(value['firstFailure'])
        self.assertEqual(len(value['cleanup']),3)
        self.assertTrue(all(row['reaped'] and row['exitCode']==0 and row['failure'] is None
                            for row in value['cleanup']))

    def test_actual_ready_timeout_retains_phase_and_recovers_exact_children(self):
        popen=subprocess.Popen;children=[]
        def launch(*_args,**kwargs):
            child=popen([sys.executable,'-c','import sys;sys.stdin.read()'],**kwargs)
            children.append(child);return child
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'receipt.json'
            with mock.patch.object(stress.subprocess,'Popen',side_effect=launch), \
                 mock.patch.object(stress,'READY_SECONDS',0.05),mock.patch('sys.stdout',new=io.StringIO()):
                with self.assertRaisesRegex(Failure,'inventory-stress-ready-timeout'):stress.workload(path)
            value=json.loads(path.read_text())
        self.assertEqual(value['firstFailure'],{'phase':'worker-ready','code':'inventory-stress-ready-timeout','kind':'failure'})
        self.assertEqual(value['readyWorkers'],0);self.assertEqual(value['barriersReleased'],0)
        self.assertGreaterEqual(value['readyElapsedSeconds'],0.05)
        self.assertTrue(all(child.returncode is not None and child.stdin.closed and child.stdout.closed
                            for child in children))
        self.assertTrue(all(row['reaped'] for row in value['cleanup']))

    def test_cleanup_only_failure_is_saved_as_primary_before_raising(self):
        cleanup=stress.cleanup_children
        def denied(children):
            rows=cleanup(children)
            rows[0]['failure']={'operation':'wait','kind':'permission'}
            return rows
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'receipt.json'
            with mock.patch.object(stress,'SECONDS',0.03),mock.patch.object(stress,'WORKER_MIB',1), \
                 mock.patch.object(stress,'cleanup_children',side_effect=denied), \
                 mock.patch('sys.stdout',new=io.StringIO()):
                with self.assertRaisesRegex(Failure,'inventory-stress-child-cleanup'):stress.workload(path)
            value=json.loads(path.read_text())
        self.assertEqual(value['status'],'failed');self.assertEqual(value['phase'],'cleanup')
        self.assertEqual(value['firstFailure'],{'phase':'cleanup','code':'inventory-stress-child-cleanup','kind':'failure'})
        self.assertTrue(all(row['reaped'] for row in value['cleanup']))

    def test_actual_worker_nonzero_keeps_precleanup_exit_status(self):
        popen=subprocess.Popen
        def launch(args,**kwargs):
            if '--worker' in args:
                return popen([sys.executable,'-c',"import sys;print('ready',flush=True);sys.stdin.read(1);sys.exit(7)"],**kwargs)
            return popen(args,**kwargs)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'receipt.json'
            with mock.patch.object(stress.subprocess,'Popen',side_effect=launch), \
                 mock.patch.object(stress,'SECONDS',0.03),mock.patch('sys.stdout',new=io.StringIO()):
                with self.assertRaisesRegex(Failure,'inventory-stress-worker-exit'):stress.workload(path)
            value=json.loads(path.read_text())
        self.assertEqual(value['firstFailure']['phase'],'worker-wait')
        self.assertEqual(value['firstFailure']['code'],'inventory-stress-worker-exit')
        self.assertTrue(all(row['exitCode']==7 for row in value['workerExitBeforeCleanup']))
        self.assertTrue(all(row['reaped'] and row['exitCode']==7 for row in value['cleanup']))

    def test_actual_eof_or_malformed_ready_cannot_release_barrier(self):
        popen=subprocess.Popen
        for script in ('pass',"print('wrong',flush=True)"):
            with tempfile.TemporaryDirectory() as directory:
                path=Path(directory)/'receipt.json'
                def launch(*_args,**kwargs):return popen([sys.executable,'-c',script],**kwargs)
                with mock.patch.object(stress.subprocess,'Popen',side_effect=launch), \
                     mock.patch('sys.stdout',new=io.StringIO()):
                    with self.assertRaisesRegex(Failure,'inventory-stress-ready$'):stress.workload(path)
                value=json.loads(path.read_text())
            self.assertEqual(value['firstFailure']['phase'],'worker-ready')
            self.assertEqual(value['barriersReleased'],0)
            self.assertTrue(all(row['reaped'] for row in value['cleanup']))

    def test_small_actual_worker_touches_memory_and_exits_naturally(self):
        result=subprocess.run([sys.executable,str(Path(stress.__file__)),'--worker','--seconds','0.02','--mib','1'],
                              check=True,capture_output=True,text=True,timeout=5)
        self.assertEqual(result.stdout.strip(),'ready')

    def test_partial_worker_launch_failure_reaps_already_owned_child(self):
        child=mock.Mock(stdout=io.StringIO(),returncode=0);child.poll.return_value=None
        with mock.patch.object(stress.subprocess,'Popen',side_effect=[child,OSError('PRIVATE')]), \
             self.assertRaises(OSError):stress.workload()
        child.terminate.assert_called_once();child.wait.assert_called_once_with(timeout=5)
        self.assertTrue(child.stdout.closed)

    def exercise(self,directory,unknown=False,cleanup='verified-gone'):
        task=Path(directory);path=task/'inventory-stress-workload.json'
        path.write_text(json.dumps({'status':'passed','phase':'finished','firstFailure':None,
            'readyWorkers':3,'barriersReleased':3,'loadSeconds':60,
            'workersReaped':3,'churnChildren':4,'churnReaped':4,'seconds':60}))
        value={'sampleComplete':not unknown,'sampleAttempts':2 if unknown else 1,
               'partialSamples':[{'unknownKinds':{'inventory-timeout':1}}] if unknown else [],
               'rssMiB':None if unknown else 4096,'cpuSeconds':None if unknown else 30,
               'inventoryBackend':'kern-proc-pgrp-helper'}
        original=mock.Mock(return_value=value)
        runner=SimpleNamespace(group_activity=original,records=[{'groupCleanup':cleanup,
            'leaderReap':{'status':'reaped','exitCode':0},'cleanupFailure':None}])
        def run(*_args,**_kwargs):
            for _ in range(20):runner.group_activity(900)
            return path
        runner.run=run
        return task,runner,original

    def test_complete_load_receipt_and_original_sampler_are_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            task,runner,original=self.exercise(directory);receipt={}
            stress.run_experiment(task,runner,{},receipt)
        self.assertEqual(receipt['status'],'passed');self.assertEqual(receipt['completeSamples'],20)
        self.assertEqual(receipt['peakRSSMiB'],4096);self.assertIs(runner.group_activity,original)

    def test_unknown_samples_and_unverified_cleanup_cannot_pass_or_erase_sampler(self):
        for unknown,cleanup in ((True,'verified-gone'),(False,'unverified')):
            with tempfile.TemporaryDirectory() as directory:
                task,runner,original=self.exercise(directory,unknown,cleanup);receipt={}
                with self.assertRaises(Failure):stress.run_experiment(task,runner,{},receipt)
            self.assertEqual(receipt['status'],'failed');self.assertIs(runner.group_activity,original)
            if unknown:
                self.assertEqual(receipt['unknownSamples'],20)
                self.assertEqual(receipt['timeoutAttempts'],20)
                self.assertIsNone(receipt['peakRSSMiB'])

    def test_incomplete_start_barrier_or_load_duration_cannot_pass(self):
        for mutation in ({'readyWorkers':2},{'barriersReleased':2},{'loadSeconds':59.9}):
            with tempfile.TemporaryDirectory() as directory:
                task,runner,original=self.exercise(directory);receipt={}
                path=task/'inventory-stress-workload.json'
                value=json.loads(path.read_text());value.update(mutation);path.write_text(json.dumps(value))
                with self.assertRaisesRegex(Failure,'inventory-stress-workload'):
                    stress.run_experiment(task,runner,{},receipt)
                self.assertIs(runner.group_activity,original)

    def test_one_cleanup_denial_does_not_skip_other_child_or_replace_primary_failure(self):
        first=mock.Mock(stdout=io.StringIO(),returncode=None);first.poll.return_value=None
        first.terminate.side_effect=PermissionError('PRIVATE')
        second=mock.Mock(stdout=io.StringIO(),returncode=0);second.poll.return_value=0
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'receipt.json'
            primary=OSError('ORIGINAL_PRIVATE')
            with mock.patch.object(stress.subprocess,'Popen',side_effect=[first,second,primary]):
                with self.assertRaises(OSError) as caught:stress.workload(path)
            value=json.loads(path.read_text())
        self.assertIs(caught.exception,primary)
        second.wait.assert_called_once_with(timeout=5)
        self.assertEqual(value['status'],'failed')
        self.assertEqual(value['cleanup'][0]['failure'],{'operation':'terminate','kind':'permission'})
        self.assertTrue(value['cleanup'][1]['reaped']);self.assertNotIn('PRIVATE',json.dumps(value))

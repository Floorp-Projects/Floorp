# SPDX-License-Identifier: MPL-2.0
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import inventory_stress as stress
from support import Failure


class InventoryStressTests(unittest.TestCase):
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
        path.write_text(json.dumps({'status':'passed','workersReaped':3,'churnChildren':4,'churnReaped':4,'seconds':60}))
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

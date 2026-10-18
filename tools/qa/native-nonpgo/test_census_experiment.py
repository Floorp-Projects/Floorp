# SPDX-License-Identifier: MPL-2.0
import contextlib
import io
import json
import sys
import time
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import census_experiment as ce
from support import Failure


class ExperimentCleanup(unittest.TestCase):
    def test_background_origins_keep_same_baseline_all_counts_and_stable_opaque_tags(self):
        baseline={(8675309,123,4,501,501)};observed=set()
        rows={(pid,124,4,501,501) for pid in range(8675310,8675330)}
        before=set(baseline);reader=mock.Mock()
        reader.tag.side_effect=lambda row:format(row[0]-8675309,'016x')
        def measure(actual,value,seen):
            self.assertIs(actual,baseline);self.assertIs(seen,observed)
            value.update(status='rejected',newCandidateIdentities=len(rows));return rows
        with mock.patch.object(ce,'measure_owned_boundary',side_effect=measure), \
             mock.patch.object(ce,'probe',return_value=reader), \
             mock.patch.object(ce,'describe',return_value={'status':'identity-verified','role':'metadata-worker',
                                                         'writerRoleEstablished':False}) as hint, \
             mock.patch('darwin_origin.describe_origin',return_value={'status':'identity-verified',
                 'classification':'pid1-original-parent-consistent','writerRoleEstablished':False,
                 'exclusionAuthorized':False,'signalOwnershipGranted':False}):
            first=ce.observe(baseline,observed,origins=True)
            second=ce.observe(baseline,observed,origins=True)
        self.assertEqual(baseline,before)
        self.assertEqual(first['newCandidateIdentities'],20);self.assertEqual(first['status'],'rejected')
        origin=first['originalParentObservations']
        self.assertEqual(origin['candidateIdentities'],20);self.assertEqual(origin['sampledCandidates'],16)
        self.assertTrue(origin['truncated']);self.assertFalse(origin['exclusionAuthorized'])
        self.assertIs(origin['checkpointEligibilityNotEvaluated'],True)
        self.assertEqual(origin,second['originalParentObservations'])
        self.assertEqual(hint.call_args.args[1],baseline|rows)
        self.assertNotIn('86753',json.dumps(first))

    def test_background_origin_unknown_and_primary_failure_never_qualify_or_refresh(self):
        baseline={(41,123,4,501,501)};rows={(42,124,4,501,501)};reader=mock.Mock()
        reader.tag.return_value='a'*16
        def measure(actual,value,seen):
            value.update(status='rejected',newCandidateIdentities=1);return rows
        with mock.patch.object(ce,'measure_owned_boundary',side_effect=measure), \
             mock.patch.object(ce,'probe',return_value=reader),mock.patch.object(ce,'describe',return_value={}), \
             mock.patch('darwin_origin.describe_origin',return_value={'status':'diagnostic-unavailable',
                'anchorQuery':{'bytes':0,'expectedBytes':56,'errno':1},'exclusionAuthorized':False}):
            value=ce.observe(baseline,set(),origins=True)
        self.assertEqual(value['status'],'rejected')
        self.assertEqual(value['originalParentObservations']['records'][0]['origin']['status'],'diagnostic-unavailable')
        value={}
        with mock.patch.object(ce,'measure_owned_boundary',side_effect=Failure('census-sysctl-permission')), \
             mock.patch.object(ce,'probe') as probe,self.assertRaisesRegex(Failure,'census-sysctl-permission'):
            ce.observe(baseline,set(),value,origins=True)
        probe.assert_not_called();self.assertNotIn('originalParentObservations',value)

    def test_early_failure_preserves_unverified_requested_sdk_as_unknown(self):
        for requested in (False,True):
            with tempfile.TemporaryDirectory() as temporary,contextlib.redirect_stdout(io.StringIO()):
                task=Path(temporary);(task/'started').write_text(str(time.time()))
                argv=['census_experiment.py','--task',str(task),'--expected-tests','282']
                if requested:argv.append('--verify-sdk')
                with mock.patch.object(sys,'argv',argv),mock.patch.object(ce.sys,'platform','linux'):
                    self.assertEqual(ce.main(),1)
                value=json.loads((task/'census-experiment.json').read_text())
            self.assertEqual(value['sdkAcquisitionRequested'],requested)
            if requested:self.assertIsNone(value['sdkAcquired'])
            else:self.assertIs(value['sdkAcquired'],False)
            self.assertEqual(value['firstFailure']['code'],'census-experiment-platform')

    def test_primary_unknown_receipt_survives_before_exception_propagates(self):
        value={}
        with mock.patch.object(ce,'owned_processes',side_effect=Failure('census-sysctl-permission')), \
                self.assertRaisesRegex(Failure,'census-sysctl-permission'):
            ce.take_owned(value)
        self.assertEqual(value,{'status':'unavailable','candidateIdentities':None})
        value={}
        def unavailable(_baseline,receipt,_observed):
            receipt.update(status='unavailable',newCandidateIdentities=None)
            raise Failure('census-sysctl-permission')
        with mock.patch.object(ce,'verify_owned_boundary',side_effect=unavailable), \
                self.assertRaisesRegex(Failure,'census-sysctl-permission'):
            ce.observe(set(),set(),value)
        self.assertEqual(value,{'status':'unavailable','newCandidateIdentities':None})

    def test_denied_cleanup_retains_original_failure_and_final_receipt(self):
        child=mock.Mock(returncode=None)
        child.poll.return_value=None
        child.terminate.side_effect=PermissionError('PRIVATE-TEXT')
        original={'stage':'known-child','code':'original'}
        runner=SimpleNamespace(failure=original,monitor_failure=None,monitor_diagnostics=[],records=[],elapsed=lambda:0)
        runner.fail=lambda *_args:None
        value={'status':'failed'}
        with tempfile.TemporaryDirectory() as temporary,contextlib.redirect_stdout(io.StringIO()) as output:
            task=Path(temporary)
            ce.finish_result(value,runner,task,child)
            receipt=json.loads((task/'census-experiment.json').read_text())
        self.assertEqual(receipt['firstFailure'],original)
        self.assertFalse(receipt['failureChildCleanup']['leaderReaped'])
        self.assertEqual(receipt['failureChildCleanup']['cleanupFailure'],{'operation':'terminate','kind':'permission'})
        self.assertNotIn('PRIVATE-TEXT',output.getvalue())
        child.kill.assert_not_called()

    def test_recovered_monitor_diagnostics_survive_finish_and_later_good_sample(self):
        with tempfile.TemporaryDirectory() as temporary,contextlib.redirect_stdout(io.StringIO()):
            task=Path(temporary)
            runner=ce.Runner(task,time.time(),{'stallQuietMinutes':20})
            for index in range(19):
                runner.record_monitor('inventory-stress',{'sampleComplete':True,'sampleAttempts':2,
                    'partialSamples':[{'unknownKinds':{'inventory-timeout':1},
                        'queryFailure':{'helperProgress':{'phase':'project-imports','events':1}}}],
                    'sequence':index})
            runner.records=[{'stage':'inventory-stress','lastActivity':{'sampleComplete':True,'sampleAttempts':1}}]
            original={'stage':'inventory-stress','code':'inventory-stress-monitor'}
            runner.failure=original
            stopping={'stage':'earlier-stop','sampleComplete':False,'sampleAttempts':3}
            runner.monitor_failure=stopping
            ce.finish_result({'status':'failed'},runner,task,None)
            value=json.loads((task/'census-experiment.json').read_text())
        self.assertEqual(len(value['monitorDiagnostics']),16)
        self.assertEqual([row['sequence'] for row in value['monitorDiagnostics']],list(range(3,19)))
        self.assertEqual(value['monitorDiagnostics'][-1]['partialSamples'][0]['queryFailure']['helperProgress'],
                         {'phase':'project-imports','events':1})
        self.assertEqual(value['firstFailure'],original);self.assertEqual(value['monitorFailure'],stopping)

    def test_second_wait_timeout_is_retained_and_never_claims_reap(self):
        child=mock.Mock(returncode=None)
        child.poll.return_value=None
        child.wait.side_effect=subprocess.TimeoutExpired(['PRIVATE-ARGV'],5)
        with mock.patch.object(ce.Runner,'group_alive',return_value=False):
            value=ce.cleanup_child(child)
        self.assertFalse(value['leaderReaped'])
        self.assertEqual(value['cleanupFailure'],{'operation':'wait-after-kill','kind':'timeout'})
        self.assertNotIn('PRIVATE-ARGV',json.dumps(value))
        child.terminate.assert_called_once()
        child.kill.assert_called_once()

    def test_reaped_child_never_receives_another_signal(self):
        child=mock.Mock(returncode=0)
        child.poll.return_value=0
        with mock.patch.object(ce.Runner,'group_alive',return_value=False):
            value=ce.cleanup_child(child)
        self.assertTrue(value['leaderReaped'])
        self.assertEqual(value['groupCleanup'],'verified-gone')
        self.assertIsNone(value['cleanupFailure'])
        child.terminate.assert_not_called()
        child.kill.assert_not_called()

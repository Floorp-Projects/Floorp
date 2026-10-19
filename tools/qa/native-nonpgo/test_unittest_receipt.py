# SPDX-License-Identifier: MPL-2.0
from pathlib import Path
import json
import tempfile
import unittest

from unittest_receipt import receipt,held_child_public,held_child_control_passed,held_resource_public,HELD_CHILD_CASE


class UnittestReceiptGuards(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name)/'unittest.private.log'

    def held_value(self,outcome='safely-rejected-deadline'):
        value={'schema':1,'status':'passed','outcome':outcome,'receiptStatus':'failed',
            'phaseFailure':'census-wait-phase-deadline','childBindingVerified':True,'childReleased':True,
            'childExitVerifiedBeforeBoundary':True,'childReaped':True,'childExitCode':0,
            'childGroupGone':True,'childCleanupComplete':True,'monitorFailureAbsent':True,
            'baselineReset':False,'candidatesExcluded':False,'signalsSent':False,'finalBoundaryZero':False,
            'checkpointEligibilityEstablished':False,
            'baselineCandidateIdentities':269,
            'lastResource':{'status':'measured','sampleComplete':True,'rssMiB':20.,'cpuSeconds':3.,
                            'projectionUnavailable':False},
            'sampleCounts':[[.1,1,0,True,True,True],[59.,2,0,True,True,False]]}
        if outcome=='natural-zero':
            value.update(receiptStatus='passed',phaseFailure=None,finalBoundaryZero=True)
            value['sampleCounts'][-1][1]=0
        return value

    def write_held(self,value,extra=''):
        marker=json.dumps({'heldChildBoundaryControl':value})
        self.path.write_text(HELD_CHILD_CASE+' ... '+marker+'\nok\n'+extra+'Ran 1 test in 1.0s\nOK\n')
        return marker

    def test_held_child_natural_zero_and_safe_deadline_are_distinct_nonqualification_controls(self):
        for outcome in ('natural-zero','safely-rejected-deadline'):
            value=self.held_value(outcome);value.update(rawPID=8675309,private='PRIVATE-HELD')
            self.write_held(value)
            result=receipt(self.path)
            self.assertTrue(result['complete'])
            control=result['heldChildBoundaryDiagnostics'][0]
            self.assertEqual(control['outcome'],outcome);self.assertTrue(held_child_control_passed(control))
            self.assertIs(control['checkpointEligibilityEstablished'],False)
            self.assertNotIn('PRIVATE-HELD',json.dumps(result));self.assertNotIn('8675309',json.dumps(result))

    def test_held_child_unknown_unreleased_unreaped_or_waived_candidates_cannot_pass(self):
        changes=[{'sampleCounts':[]},{'sampleCounts':[[.1,1,0,True,True,True]]},
            {'childReleased':False},{'childBindingVerified':False},{'childExitVerifiedBeforeBoundary':False},
            {'childReaped':False},{'childGroupGone':False},{'childCleanupComplete':False},{'childExitCode':-15},
            {'monitorFailureAbsent':False},{'baselineReset':True},{'candidatesExcluded':True},{'signalsSent':True},
            {'finalBoundaryZero':True},{'phaseFailure':None},{'outcome':'failed'},
            {'lastResource':{'status':'unavailable','sampleComplete':True,'rssMiB':20.,'cpuSeconds':3.,
                             'projectionUnavailable':False}},
            {'lastResource':{'status':'measured','sampleComplete':True,'rssMiB':None,'cpuSeconds':None,
                             'projectionUnavailable':False}},
            {'sampleCounts':[[59.,1,0,True,True,True],[1.,2,0,True,True,False]]},
            {'sampleCounts':[[.1,1,0,True,True,True],[59.,0,0,True,True,False]]},
            {'sampleCounts':[[.1,1,0,True,True,True],[59.,2,0,False,False,False]]},
            {'sampleCounts':[[.1,1,0,True,True,True],[59.,2,0,True,True,True]]},
            {'sampleCounts':[[.1,1,0,True,True,True],[59.,None,None,False,False,None]]}]
        for change in changes:
            value={**self.held_value(),**change}
            self.assertFalse(held_child_control_passed(value),change)
            self.write_held(value)
            self.assertFalse(receipt(self.path)['complete'],change)
        for samples in ([[.1,0,0,True,True,True],[1.,0,0,True,True,False]],
                        [[.1,1,0,True,True,True],[1.,0,0,True,True,False],[2.,0,0,True,True,False]]):
            value=self.held_value('natural-zero');value['sampleCounts']=samples
            self.assertFalse(held_child_control_passed(value))
            self.write_held(value);self.assertFalse(receipt(self.path)['complete'])

    def test_held_child_missing_duplicate_malformed_or_false_qualification_are_unknown(self):
        self.path.write_text(HELD_CHILD_CASE+' ... ok\nRan 1 test in 1.0s\nOK\n')
        self.assertFalse(receipt(self.path)['complete'])
        marker=json.dumps({'heldChildBoundaryControl':self.held_value()})
        self.write_held(self.held_value(),marker+'\n')
        self.assertFalse(receipt(self.path)['complete'])
        for change in ({'schema':True},{'checkpointEligibilityEstablished':True},
                       {'outcome':'PRIVATE-OUTCOME'},{'childReleased':'PRIVATE-BOOL'},
                       {'sampleCounts':[[float('nan'),2,0,True,True,False]]},
                       {'sampleCounts':[[0.,2,0,True,True,False]]*129}):
            self.write_held({**self.held_value(),**change})
            result=receipt(self.path)
            self.assertFalse(result['complete']);self.assertNotIn('PRIVATE-',json.dumps(result))

    def test_held_child_failed_unknown_samples_remain_visible_without_raw_details(self):
        value=self.held_value();value.update(status='failed',outcome='failed',phaseFailure=None)
        value['sampleCounts'][-1]=[1.,None,None,False,False,None]
        value['lastResource']={'status':'unavailable','sampleComplete':False,'rssMiB':None,'cpuSeconds':None}
        projected=held_child_public(value)
        self.assertFalse(held_child_control_passed(projected))
        self.assertIsNone(projected['sampleCounts'][-1][1])
        self.write_held(value)
        self.assertFalse(receipt(self.path)['complete'])
        method='test_actual_held_child_rejects_until_exit_and_preserves_unrelated_candidates'
        self.path.write_text(HELD_CHILD_CASE+' ... '+json.dumps({'heldChildBoundaryControl':value})+'\n'
            'ERROR: '+method+' (test_census_convergence.ConvergenceGuards.'+method+')\n'
            'support.Failure: monitor-unavailable\nRan 1 test in 1.0s\nFAILED (errors=1)\n')
        result=receipt(self.path)
        self.assertTrue(result['complete'])
        self.assertEqual(result['heldChildBoundaryDiagnostics'][0]['status'],'failed')

    def test_held_resource_keeps_stop_queries_pause_and_bound_diagnostics_without_raw_fields(self):
        value={'status':'unavailable','sampleComplete':False,'rssMiB':None,'cpuSeconds':None,
            'operation':'candidate-task-recheck','reason':'short-task-info',
            'failureCode':'census-wait-resource-unavailable','candidateIndex':7,
            'taskQuery':{'bytes':0,'expectedBytes':232,'errno':3,'rawPID':8675309},
            'retryBSDObservation':{'status':'sleeping','inExit':True,'private':'PRIVATE-BSD'},
            'resourceRetryPause':{'maximumPauses':1,'pauses':1,'requestedSeconds':.25,
                'elapsedSeconds':.251,'status':'completed','private':'PRIVATE-PAUSE'},
            'readDiagnostics':[{'status':'unavailable','reason':'short-task-info','sameCandidateBirth':True,
                'taskQuery':{'bytes':0,'expectedBytes':232,'errno':3},'fallbackReason':'measured',
                'fallbackBSDQuery':{'bytes':136,'expectedBytes':136,'errno':0},
                'private':'PRIVATE-TRACE','identityTag':'PRIVATE-TAG'}],
            '_pids':[8675309],'argv':'PRIVATE-ARGV','identityTag':'PRIVATE-ID'}
        result=held_resource_public(value)
        self.assertEqual(result['taskQuery'],{'bytes':0,'expectedBytes':232,'errno':3})
        self.assertEqual(result['resourceRetryPause']['elapsedSeconds'],.251)
        self.assertTrue(result['readDiagnostics'][0]['sameCandidateBirth'])
        self.assertEqual(result['retryBSDObservation'],{'status':'sleeping','inExit':True})
        self.assertEqual(result['failureCode'],'census-wait-resource-unavailable')
        self.assertIsNone(result['rssMiB']);self.assertIsNone(result['cpuSeconds'])
        self.assertNotIn('PRIVATE-',json.dumps(result));self.assertNotIn('8675309',json.dumps(result))

    def test_held_resource_malformed_counters_pause_or_trace_are_unknown(self):
        for resource in ({'rssMiB':float('nan')},{'cpuSeconds':True},{'readDiagnostics':[{}]*3},
                         {'resourceRetryPause':{'maximumPauses':True,'pauses':2}},
                         {'resourceRetryPause':{'maximumPauses':1,'pauses':1,'requestedSeconds':.25,
                             'elapsedSeconds':float('inf'),'status':'completed'}}):
            value=self.held_value();value['lastResource']=resource
            self.write_held(value)
            self.assertFalse(receipt(self.path)['complete'])

    def test_failure_identity_type_code_and_frames_survive_without_private_text(self):
        frame = Path(__file__).with_name('process_census.py').resolve()
        self.path.write_text('ERROR: test_real_mac_new_owned_process_is_observed '
            '(test_checkpoint.CheckpointGuards.test_real_mac_new_owned_process_is_observed)\n'
            'Traceback (most recent call last):\n'
            f'  File "{frame}", line 100, in decode\n'
            'support.Failure: census-sysctl-permission\n'
            'ERROR: test_caps_stop_before_ready_or_upload '
            '(test_checkpoint.CheckpointGuards.test_caps_stop_before_ready_or_upload)\n'
            'PermissionError: [Errno 13] PRIVATE-SENTINEL token=https://private.invalid/signed\n'
            'Ran 178 tests in 67.0s\nFAILED (errors=2)\n')
        value = receipt(self.path)
        self.assertTrue(value['complete'])
        self.assertEqual(value['errors'], 2)
        self.assertEqual(value['cases'][0]['failureCode'], 'census-sysctl-permission')
        self.assertEqual(value['cases'][0]['frames'][0]['file'], 'process_census.py')
        self.assertEqual(value['cases'][1]['exceptionType'], 'PermissionError')
        self.assertEqual(value['cases'][1]['errno'], 13)
        import json
        for secret in ('PRIVATE-SENTINEL','private.invalid','token=',str(frame.parent)):
            self.assertNotIn(secret, json.dumps(value))

    def test_complete_success_and_skips_remain_distinguishable(self):
        for ending, skipped in (('OK',0),('OK (skipped=10)',10)):
            self.path.write_text('Ran 178 tests in 7.7s\n'+ending+'\n')
            value = receipt(self.path)
            self.assertTrue(value['complete'])
            self.assertEqual(value['total'],178)
            self.assertEqual(value['skipped'],skipped)

    def test_missing_summary_truncated_line_symlink_and_wrong_case_counts_are_unknown(self):
        for text in ('ERROR: private', 'Ran 178 tests in 1.0s\n',
                     'Ran 178 tests in 1.0s\nFAILED (errors=1)\n'):
            self.path.write_text(text)
            value = receipt(self.path)
            self.assertFalse(value['complete'])
            self.assertIsNone(value['errors'])
        self.path.unlink()
        self.path.symlink_to(Path(__file__).resolve())
        self.assertFalse(receipt(self.path)['complete'])

    def test_unknown_identifier_or_phase_is_never_published(self):
        import json
        for header in ('ERROR: PRIVATE_SENTINEL_PHASE (test_checkpoint.PRIVATE_SENTINEL_CLASS.PRIVATE_SENTINEL_METHOD)',
                       'ERROR: PRIVATE_SENTINEL_PHASE (test_checkpoint.CheckpointGuards.test_real_mac_new_owned_process_is_observed)'):
            self.path.write_text(header+'\nAssertionError: PRIVATE_SENTINEL_MESSAGE\n'
                                 'Ran 1 test in 1.0s\nFAILED (errors=1)\n')
            value = receipt(self.path)
            self.assertFalse(value['complete'])
            self.assertNotIn('PRIVATE_SENTINEL',json.dumps(value))

    def test_subtest_parameters_function_text_and_invalid_frame_lines_are_omitted(self):
        import json
        file = Path(__file__).with_name('checkpoint.py').resolve()
        self.path.write_text('ERROR: test_real_mac_new_owned_process_is_observed '
            '(test_checkpoint.CheckpointGuards.test_real_mac_new_owned_process_is_observed) '
            '(token="PRIVATE_SENTINEL_PARAMETER")\n'
            f'  File "{file}", line 1, in PRIVATE_SENTINEL_FUNCTION\n'
            f'  File "{file}", line 999999999999, in PRIVATE_SENTINEL_FUNCTION\n'
            'AssertionError: PRIVATE_SENTINEL_MESSAGE\nRan 1 test in 1.0s\nFAILED (errors=1)\n')
        value = receipt(self.path)
        self.assertTrue(value['complete'])
        self.assertEqual(value['cases'][0]['frames'],[{'file':'checkpoint.py','line':1}])
        self.assertNotIn('PRIVATE_SENTINEL',json.dumps(value))

    def test_recording_failure_identity_and_fixed_frames_survive_without_private_data(self):
        root = Path(__file__).resolve().parents[2] / 'app-shim'
        alias = self.path.parent / 'PRIVATE-SENTINEL-ALIAS.py'
        alias.symlink_to(root / 'shim_failure_diagnostics.py')
        method = 'test_event_history_keeps_first_and_terminal_separately_from_recent_sixteen'
        self.path.write_text('FAIL: '+method+' (test_shim_failure_diagnostics.ProjectionTests.'+method+')\n'
            f'  File "{root / "test_shim_failure_diagnostics.py"}", line 1, in {method}\n'
            f'  File "{root / "shim_failure_diagnostics.py"}", line 1, in event_history\n'
            f'  File "{alias}", line 1, in PRIVATE_FUNCTION\n'
            f'  File "{self.path.parent / "shim_failure_diagnostics.py"}", line 1, in PRIVATE_FUNCTION\n'
            'AssertionError: PRIVATE-SENTINEL-NONCE\nRan 1 test in 0.1s\nFAILED (failures=1)\n')
        self.assertFalse(receipt(self.path)['complete'])
        value = receipt(self.path, recording=True)
        self.assertTrue(value['complete'])
        self.assertEqual(value['status'], 'failed')
        self.assertEqual(value['cases'][0]['test'], 'test_shim_failure_diagnostics.ProjectionTests.'+method)
        self.assertEqual(value['cases'][0]['exceptionType'], 'AssertionError')
        self.assertEqual(value['cases'][0]['frames'], [
            {'file':'test_shim_failure_diagnostics.py','line':1},
            {'file':'shim_failure_diagnostics.py','line':1},
            {'file':'shim_failure_diagnostics.py','line':1}])
        self.assertNotIn('PRIVATE', json.dumps(value))
        self.assertNotIn(str(root), json.dumps(value))

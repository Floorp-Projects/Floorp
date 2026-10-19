# SPDX-License-Identifier: MPL-2.0
from pathlib import Path
import json
import tempfile
import unittest

from unittest_receipt import (receipt,held_child_public,held_child_control_passed,held_resource_public,
    HELD_CHILD_CASE,HELPER_WORKER_CASE,HELPER_WORKER_ID,HELPER_WORKER_METHOD,
    DENSITY_CASE,DENSITY_ID,DENSITY_METHOD,density_public)


class UnittestReceiptGuards(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name)/'unittest.private.log'

    def density_value(self, observed=False):
        return {'schema':1,'case':DENSITY_ID,'status':'observed' if observed else 'unconfirmed',
            'reason':'build-completion-profile-parsed' if observed else 'build-completion-processing-limit',
            'elapsedSeconds':8.25 if observed else 30.001,'inputBytes':195000000,
            'inputSHA256':'a'*64,'resourceRows':1000000,'processingSeconds':30,
            'completeCoverage':observed,'objectCount':1 if observed else None,
            'compileCount':1 if observed else None}

    def write_density(self,value,failed=True):
        marker=json.dumps({'buildCompletionDensityDiagnostic':value})
        ending=('FAIL\nFAIL: '+DENSITY_METHOD+' ('+DENSITY_ID+')\n'
            'AssertionError: PRIVATE-ASSERTION /Users/private/input\n'
            'Ran 1 test in 31.0s\nFAILED (failures=1)\n') if failed else (
            'ok\nRan 1 test in 9.0s\nOK\n')
        self.path.write_text(DENSITY_CASE+' ... '+marker+'\n'+ending)
        return marker

    def test_density_failed_observation_retains_reason_and_elapsed_without_exception_text(self):
        value=self.density_value();self.write_density(value)
        result=receipt(self.path)
        self.assertTrue(result['complete']);self.assertEqual(result['failures'],1)
        self.assertEqual(result['buildCompletionDensityDiagnostics'],[value])
        self.assertNotIn('PRIVATE',json.dumps(result));self.assertNotIn('/Users/',json.dumps(result))

    def test_density_observed_control_retains_positive_counts_and_passes(self):
        value=self.density_value(True);self.write_density(value,failed=False)
        result=receipt(self.path)
        self.assertTrue(result['complete']);self.assertEqual(result['status'],'passed')
        self.assertEqual(result['buildCompletionDensityDiagnostics'],[value])

    def test_density_missing_duplicate_truncated_or_foreign_marker_stays_unknown(self):
        marker=self.write_density(self.density_value());original=self.path.read_text()
        for text in (original.replace(marker,''),original.replace('\nFAIL\n','\n'+marker+'\nFAIL\n'),
                original.replace(marker,marker[:-1]),original.replace(DENSITY_CASE+' ... ','')):
            self.path.write_text(text);self.assertFalse(receipt(self.path)['complete'])

    def test_density_unconfirmed_marker_cannot_qualify_a_passed_suite(self):
        self.write_density(self.density_value(),failed=False)
        self.assertFalse(receipt(self.path)['complete'])

    def test_density_projection_rejects_unbounded_private_or_mistyped_fields(self):
        for change in ({'schema':True},{'case':'PRIVATE-CASE'},{'reason':'PRIVATE-REASON'},
                {'elapsedSeconds':True},{'elapsedSeconds':None},{'elapsedSeconds':float('nan')},
                {'elapsedSeconds':float('inf')},{'elapsedSeconds':-1},{'elapsedSeconds':901},
                {'inputBytes':True},{'inputBytes':16*1024**2},{'inputBytes':2*1024**3+1},
                {'inputSHA256':'PRIVATE-SHA'},{'resourceRows':True},{'resourceRows':999999},
                {'processingSeconds':True},{'processingSeconds':31},
                {'completeCoverage':None},{'objectCount':0},{'compileCount':False}):
            self.write_density({**self.density_value(),**change})
            self.assertFalse(receipt(self.path)['complete'],change)
        value=self.density_value();value['private']='/Users/private/token'
        self.write_density(value);self.assertFalse(receipt(self.path)['complete'])

    def test_density_projection_rejects_false_observation_and_missing_fields(self):
        for key in self.density_value():
            value=self.density_value();value.pop(key);self.write_density(value)
            self.assertFalse(receipt(self.path)['complete'],key)
        for change in ({'reason':'build-completion-processing-limit'},{'objectCount':True},
                {'compileCount':0},{'completeCoverage':False}):
            self.write_density({**self.density_value(True),**change},failed=False)
            self.assertFalse(receipt(self.path)['complete'])

    def test_density_skipped_case_requires_no_diagnostic(self):
        self.path.write_text(DENSITY_CASE+' ... skipped PRIVATE-REASON\n'
            'Ran 1 test in 1.0s\nOK (skipped=1)\n')
        value=receipt(self.path)
        self.assertTrue(value['complete']);self.assertEqual(value['skipped'],1)
        self.assertEqual(value['buildCompletionDensityDiagnostics'],[])

    def test_density_allowlisted_other_guard_reason_is_retained_without_becoming_zero(self):
        for reason in ('file-changed','profile-missing','profile-ambiguous'):
            value={**self.density_value(),'reason':'build-completion-'+reason,'elapsedSeconds':.25}
            self.assertEqual(density_public(value),value)
            self.write_density(value);result=receipt(self.path)
            self.assertTrue(result['complete']);self.assertIsNone(result['buildCompletionDensityDiagnostics'][0]['objectCount'])

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

    def worker_value(self):
        from test_helper_import_preflight import control_value
        return control_value()

    def write_worker(self,value,*,failed=False,extra=''):
        marker=json.dumps({'helperImportControlDiagnostic':value})
        ending=('FAIL\nFAIL: '+HELPER_WORKER_METHOD+' ('+HELPER_WORKER_ID+')\n'
            'AssertionError: PRIVATE-ASSERTION\nRan 1 test in 1.0s\nFAILED (failures=1)\n') if failed else (
            'ok\n'+extra+'Ran 1 test in 1.0s\nOK\n')
        self.path.write_text(HELPER_WORKER_CASE+' ... '+marker+'\n'+ending)
        return marker

    def test_helper_worker_success_is_case_bound_and_idempotent_without_private_extras(self):
        from helper_import_preflight import worker_control_projection
        value=self.worker_value();value.update(rawPID=8675309,private='PRIVATE-WORKER')
        for row in value['workerMeasurements']+value['cleanup']:
            row.update(argv='PRIVATE-ARGV',path='PRIVATE-PATH',rawPID=8675309)
        self.write_worker(value)
        result=receipt(self.path)
        self.assertTrue(result['complete'])
        control=result['helperWorkerControlDiagnostics'][0]
        self.assertEqual(control['case'],HELPER_WORKER_ID)
        self.assertEqual(control['mode'],'fixed-iterations')
        self.assertEqual(control['iterations'],4)
        self.assertEqual(control['temporaryDirectoryCleanup']['status'],'removed-after-exact-reap')
        self.assertEqual(worker_control_projection(control),control)
        self.assertNotIn('PRIVATE-',json.dumps(result));self.assertNotIn('8675309',json.dumps(result))

    def test_helper_worker_failure_preserves_primary_and_independent_cleanup(self):
        value=self.worker_value();value.update(status='failed',firstFailure={
            'phase':'worker-ready','code':'helper-import-worker-ready-timeout',
            'kind':'timeout','errno':None,'failureCode':None,'private':'PRIVATE-FIRST'})
        value['cleanup'][0]['failure']={'operation':'wait','kind':'os-error','errno':5,
            'failureCode':None,'private':'PRIVATE-CLEANUP'}
        value['temporaryDiskRemoval']={'status':'unverified','failure':None}
        value['temporaryDirectoryCleanup']={'status':'deferred-unverified','failure':None}
        self.write_worker(value,failed=True)
        result=receipt(self.path)
        self.assertTrue(result['complete']);self.assertEqual(result['failures'],1)
        control=result['helperWorkerControlDiagnostics'][0]
        self.assertEqual(control['firstFailure']['phase'],'worker-ready')
        self.assertEqual(control['firstFailure']['code'],'helper-import-worker-ready-timeout')
        self.assertEqual(control['cleanup'][0]['failure']['errno'],5)
        self.assertEqual(control['temporaryDiskRemoval']['status'],'unverified')
        self.assertEqual(control['temporaryDirectoryCleanup']['status'],'deferred-unverified')
        self.assertNotIn('PRIVATE-',json.dumps(result))

    def test_helper_worker_missing_duplicate_truncated_or_wrong_case_marker_is_unknown(self):
        marker=self.write_worker(self.worker_value());original=self.path.read_text()
        for text in (original.replace(marker,''),original.replace('\nok\n','\n'+marker+'\nok\n'),
            original.replace(marker,marker[:-1]),
            original.replace(HELPER_WORKER_CASE+' ... ',''),
            original.replace(HELPER_WORKER_CASE,'test_unknown (test_unknown.Case.test_unknown)')):
            self.path.write_text(text)
            self.assertFalse(receipt(self.path)['complete'])

    def test_helper_worker_malformed_private_or_false_pass_cannot_qualify(self):
        for change in ({'schema':True},{'case':'PRIVATE-CASE'},{'mode':'duration'},
            {'iterations':True},{'valid':False},{'readyWorkers':float('nan')},
            {'workerMeasurements':[]},{'cleanup':[]},
            {'temporaryDirectoryCleanup':{'status':'deferred-unverified','failure':None}},
            {'temporaryDiskRemoval':{'status':'unverified','failure':None}}):
            self.write_worker({**self.worker_value(),**change})
            value=receipt(self.path)
            self.assertFalse(value['complete']);self.assertNotIn('PRIVATE-',json.dumps(value))
        value=self.worker_value();value['status']='failed'
        self.write_worker(value)
        self.assertFalse(receipt(self.path)['complete'])

    def test_helper_worker_skipped_case_requires_no_marker(self):
        self.path.write_text(HELPER_WORKER_CASE+' ... skipped PRIVATE-REASON\n'
            'Ran 1 test in 1.0s\nOK (skipped=1)\n')
        value=receipt(self.path)
        self.assertTrue(value['complete']);self.assertEqual(value['skipped'],1)
        self.assertEqual(value['helperWorkerControlDiagnostics'],[])

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

    def test_core_failure_identity_keeps_fixed_code_without_unknown_text(self):
        import qa_core
        import traceback
        try:
            qa_core.require(False, 'census-sysctl-permission')
        except qa_core.Failure:
            trace = traceback.format_exc()
        self.path.write_text('ERROR: test_caps_stop_before_ready_or_upload '
            '(test_checkpoint.CheckpointGuards.test_caps_stop_before_ready_or_upload)\n' + trace +
            'ERROR: test_real_mac_new_owned_process_is_observed '
            '(test_checkpoint.CheckpointGuards.test_real_mac_new_owned_process_is_observed)\n'
            'qa_core.Failure: PRIVATE-SENTINEL token=https://private.invalid/signed\n'
            'Ran 2 tests in 1.0s\nFAILED (errors=2)\n')
        value = receipt(self.path)
        self.assertTrue(value['complete'])
        self.assertEqual(value['cases'][0]['exceptionType'], 'qa_core.Failure')
        self.assertEqual(value['cases'][0]['failureCode'], 'census-sysctl-permission')
        self.assertIn('qa_core.py', [row['file'] for row in value['cases'][0]['frames']])
        self.assertNotIn('failureCode', value['cases'][1])
        for secret in ('PRIVATE-SENTINEL', 'private.invalid', 'token='):
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

# SPDX-License-Identifier: MPL-2.0
import io
import json
import tempfile
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest import mock

import native_selection_diagnostic as diag
from native_processes import NativeProcesses
from support import Failure
from unittest_receipt import receipt


class NativeSelectionDiagnostics(unittest.TestCase):
    def test_private_ready_ack_is_bounded_exact_and_never_accepts_environment_fields(self):
        self.assertEqual(diag.child_arguments(b'{"ready":true,"origArgv":["/private/python","-c","code"]}\n'),
                         ('/private/python','-c','code'))
        for raw in (b'{}\n',b'{"ready":true,"origArgv":["/a"],"ENV":"PRIVATE"}\n',
                    b'{"ready":true,"origArgv":["/a"]}',b'X'*65537):
            with self.assertRaises((Failure,ValueError)):diag.child_arguments(raw)

    def test_initial_wrapper_argv_zero_change_keeps_exact_tail_child_and_native_path_checks(self):
        actual=('/private/interpreter','-c','PRIVATE_CODE')
        command=('/private/launcher','-c','PRIVATE_CODE');details=diag.fresh()
        diag.initial_arguments(actual,command,actual,'/private/interpreter',details)
        self.assertFalse(details['initialArgv0TextMatches'])
        self.assertFalse(details['initialArgv0CanonicalMatches'])
        for key in ('initialArgvCountMatches','initialArgvTailMatches','initialArgvMatchesChildOrigArgv',
                    'childOrigArgvTailMatches','initialArgv0MatchesNativePath'):self.assertTrue(details[key],key)
        self.assertNotIn('PRIVATE',json.dumps(diag.public(details)))
        diag.initial_arguments(actual,('/private/launcher','-c','DIFFERENT'),actual,'/private/interpreter',details)
        self.assertFalse(details['initialArgvTailMatches'])
    def inventory(self):
        value=object.__new__(NativeProcesses)
        value.pids=mock.Mock(return_value=[900])
        value.bsd=mock.Mock(return_value=SimpleNamespace(pid=900,start_sec=100,start_usec=7,uid=501))
        value.path=mock.Mock(return_value='/private/native')
        value.argv=mock.Mock(return_value=['/private/native','PRIVATE_ARGV'])
        return value,(900,'100.000007',('/private/native','PRIVATE_ARGV'))

    def select(self,value,row,details):
        with mock.patch.object(diag,'child_status',return_value={'matchingChild':True,'errno':0,'exitPending':False}):
            return diag.select(value,row,'/private/native',details)

    def test_actual_selection_observations_and_original_methods_are_preserved(self):
        value,row=self.inventory();original=value.pids;details=diag.fresh()
        with mock.patch('native_processes.os.geteuid',return_value=501):
            self.assertEqual(self.select(value,row,details),[row])
        self.assertIs(value.pids,original)
        self.assertEqual(original.call_count,1)
        for key in ('candidateListed','targetFiltered','targetBirthMatched','targetPathMatched',
                    'targetArgvMatched','filteredBirthMatched','filteredArgvMatched','exactRowIncluded'):
            self.assertTrue(details[key],key)
        self.assertEqual(details['beforeState'],'live');self.assertEqual(details['afterState'],'live')
        self.assertNotIn('PRIVATE',json.dumps(diag.public(details)))

    def test_omission_filter_mismatch_gone_and_changed_row_have_distinct_predicates(self):
        for branch in ('omission','path','gone','row'):
            value,row=self.inventory();details=diag.fresh()
            if branch=='omission':value.pids.return_value=[901];value.bsd.return_value=None
            if branch=='path':value.path.return_value='/private/changed'
            if branch=='gone':value.bsd.return_value=None
            if branch=='row':value.argv.return_value=['/private/native','DIFFERENT_PRIVATE_ARGV']
            with mock.patch('native_processes.os.geteuid',return_value=501):
                self.assertNotIn(row,self.select(value,row,details))
            if branch=='omission':self.assertFalse(details['candidateListed'])
            if branch=='path':self.assertFalse(details['targetPathMatched'])
            if branch=='gone':self.assertGreater(details['targetGoneReads'],0)
            if branch=='row':self.assertTrue(details['targetFiltered']);self.assertFalse(details['filteredArgvMatched'])

    def test_original_exception_survives_lifetime_unavailability_and_wrappers_restore(self):
        value,row=self.inventory();primary=PermissionError(13,'PRIVATE');value.path.side_effect=primary
        original=value.path;details=diag.fresh()
        with mock.patch.object(diag,'child_status',side_effect=OSError(5,'PRIVATE')):
            with self.assertRaises(PermissionError) as caught:diag.select(value,row,'/private/native',details)
        self.assertIs(caught.exception,primary);self.assertIs(value.path,original)
        self.assertEqual(details['selectionAPI'],'path');self.assertEqual(details['selectionErrno'],13)
        self.assertTrue(details['selectionFailureTargetsChild'])
        self.assertIsNone(details['filteredCount'])
        self.assertEqual(details['beforeState'],'unavailable');self.assertEqual(details['afterState'],'unavailable')
        self.assertNotIn('PRIVATE',json.dumps(diag.public(details)))
        value,row=self.inventory();value.pids.return_value=[901]
        value.bsd.return_value=SimpleNamespace(pid=901,start_sec=100,start_usec=7,uid=501)
        value.path.side_effect=primary;details=diag.fresh()
        with self.assertRaises(PermissionError):self.select(value,row,details)
        self.assertFalse(details['selectionFailureTargetsChild'])

    def test_unmatched_lifetime_is_unknown_and_exit_pending_is_not_live(self):
        for state,expected in (({'matchingChild':False,'errno':0,'exitPending':False},'unavailable'),
                               ({'matchingChild':True,'errno':0,'exitPending':True},'exit-pending')):
            details=diag.fresh()
            with mock.patch.object(diag,'child_status',return_value=state):diag.lifetime(900,details,'before')
            self.assertEqual(details['beforeState'],expected)

    def test_close_failure_does_not_skip_exact_child_cleanup(self):
        child=mock.Mock();child.stdin.close.side_effect=OSError('PRIVATE')
        details=diag.fresh()
        with mock.patch('census_experiment.cleanup_child',return_value={
             'leaderReaped':True,'groupCleanup':'verified-gone','cleanupFailure':None}) as cleanup:
            self.assertFalse(diag.finish(child,details))
        cleanup.assert_called_once_with(child)
        self.assertTrue(details['streamCloseFailed']);self.assertEqual(details['phase'],'cleanup')

    def test_safe_diagnostics_reach_structured_unittest_receipt(self):
        details=diag.fresh();details.update(readyAcknowledged=True)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'test.private.log'
            path.write_text('verbose test prefix '+json.dumps({'nativeSelectionDiagnostic':details})+
                            '\nRan 1 test in 0.01s\nOK\n')
            value=receipt(path)
        self.assertTrue(value['complete']);self.assertEqual(value['nativeSelectionDiagnostics'],[details])

    def test_remaining_group_cleanup_failure_survives_public_receipt_projection(self):
        child=SimpleNamespace(stdin=io.StringIO(),stdout=io.StringIO());details=diag.fresh()
        with mock.patch('census_experiment.cleanup_child',return_value={
             'leaderReaped':True,'groupCleanup':'unverified',
             'cleanupFailure':{'operation':'group-probe','kind':'group-still-present'}}):
            self.assertFalse(diag.finish(child,details))
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'test.private.log'
            path.write_text(json.dumps({'nativeSelectionDiagnostic':diag.public(details)})+
                            '\nRan 1 test in 0.01s\nOK\n')
            value=receipt(path)
        self.assertTrue(value['complete'])
        self.assertEqual(value['nativeSelectionDiagnostics'][0]['cleanupFailureKind'],'group-still-present')
        self.assertEqual(value['nativeSelectionDiagnostics'][0]['status'],'failed')

    def test_extra_private_values_invalid_types_and_duplicate_markers_fail_closed(self):
        for details in ({**diag.fresh(),'private':'PRIVATE_SENTINEL'},
                        {**diag.fresh(),'candidateCount':True},
                        {**diag.fresh(),'selectionErrorKind':'PRIVATE_SENTINEL'}):
            with self.assertRaises(Failure):diag.public(details)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'test.private.log'
            line=json.dumps({'nativeSelectionDiagnostic':diag.fresh()})+'\n'
            path.write_text(line+line+'Ran 1 test in 0.01s\nOK\n')
            self.assertFalse(receipt(path)['complete'])

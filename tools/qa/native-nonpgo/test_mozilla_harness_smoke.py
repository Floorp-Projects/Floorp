# SPDX-License-Identifier: MPL-2.0
import copy
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest import mock

import mozilla_harness_smoke as smoke
from support import Failure


class MozillaHarnessSmokeTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix='mozilla-smoke-')
        self.addCleanup(directory.cleanup)
        self.task = Path(directory.name)
        self.parent = SimpleNamespace(monotonic_started=1.0, started=2.0,
                                      limits={'jobMinutes': 360}, failure=None)
        self.env = {'MOZ_AUTOMATION': '1', 'PATH': '/usr/bin:/bin', 'MOZ_BUILD_DATE': '20261019035607'}
        self.case_runner = SimpleNamespace(records=[], failure=None,
                                          monitor_failure=None, monitor_diagnostics=[])
        self.manifest = self.task / 'xpcshell.toml'
        self.test = self.task / 'test_current_debug.js'
        self.binary = self.task / 'xpcshell'

    def run_case(self, *, negative=False, code=None, group='verified-gone', cleanup=None,
                 monitor=None, unexpected_error=None, reaped=None, debug=True):
        if code is None:
            code = 1 if negative else 0
        reaped = code if reaped is None else reaped
        def run(stage, args, cwd, env, platform_deadline, *, phase_deadline_minutes):
            self.assertEqual(stage, 'mozilla-xpcshell')
            self.assertEqual(cwd, self.task)
            self.assertEqual(platform_deadline, 360)
            self.assertEqual(phase_deadline_minutes, 335)
            self.assertNotIn('MOZ_AUTOMATION', env)
            self.assertEqual(env['MOZ_ASSUME_NODE_RUNNING'], '1')
            self.assertEqual(self.env['MOZ_AUTOMATION'], '1')
            raw = Path(args[args.index('--log-raw') + 1])
            rows = [{'action':'suite_start', 'tests':{'xpcshell.toml':[self.test.name]}},
                    {'action':'test_start', 'test':self.test.name}]
            if negative:
                rows.append({'action':'test_status', 'test':self.test.name, 'status':'FAIL',
                             'expected':'PASS', 'subtest':smoke.FAILURE_MARKER})
            elif debug:
                rows.extend([{'action':'test_status', 'test':self.test.name, 'status':'PASS',
                              'subtest':'debug'},
                             {'action':'log', 'level':'INFO', 'message':smoke.DEBUG_MARKER}])
            rows.extend([{'action':'test_end', 'test':self.test.name,
                          'status':'FAIL' if negative else 'PASS',
                          **({'expected':'PASS'} if negative else {})},
                         {'action':'suite_end'}])
            raw.write_text(''.join(json.dumps({'time':10, **row})+'\n' for row in rows))
            self.case_runner.records.append({'stage':stage, 'exitCode':code,
                'reason':'nonzero-exit' if code else None, 'failurePhase':'wait' if code else None,
                'leaderReap':{'status':'reaped','exitCode':reaped}, 'groupCleanup':group,
                'cleanupFailure':cleanup, 'minimumFreeGiB':None, 'peakRSSMiB':None})
            self.case_runner.monitor_failure = monitor
            if code:
                self.case_runner.failure = {'stage':stage,'code':'nonzero-exit','elapsedSeconds':3}
                raise Failure('nonzero-exit')
            if unexpected_error:
                raise unexpected_error
        self.case_runner.run = run
        with mock.patch.object(smoke, 'Runner', return_value=self.case_runner):
            return smoke.run_case('case', self.task, self.binary, self.manifest, self.test,
                                  self.task, self.parent, self.env, 335,
                                  negative=negative, debug=debug and not negative)

    def test_actual_failure_control_retains_failure_and_exact_exit_one(self):
        value = self.run_case(negative=True)
        self.assertTrue(value['expectationSatisfied'])
        self.assertEqual(value['harness']['status'], 'failed')
        self.assertEqual(value['runnerFailure']['code'], 'nonzero-exit')
        self.assertEqual(value['process']['exitCode'], 1)
        self.assertEqual(value['caughtCode'], 'nonzero-exit')
        self.assertIsNone(self.parent.failure)
        self.assertEqual(self.env['MOZ_AUTOMATION'], '1')

    def test_positive_requires_real_pass_and_keeps_unmeasured_resources_unknown(self):
        value = self.run_case()
        self.assertTrue(value['expectationSatisfied'])
        self.assertEqual(value['harness']['status'], 'passed')
        self.assertIsNone(value['process']['peakRSSMiB'])
        self.assertIsNone(value['process']['minimumFreeGiB'])
        self.assertIsNone(value['runnerFailure'])

    def test_expected_failure_is_rejected_for_other_exit_crash_or_monitor_failure(self):
        for code in (0, 2, -9, False):
            with self.subTest(code=code):
                self.case_runner.records.clear()
                self.case_runner.failure = None
                if (self.task / 'mozilla-case').exists():
                    import shutil
                    shutil.rmtree(self.task / 'mozilla-case')
                self.assertFalse(self.run_case(negative=True, code=code)['expectationSatisfied'])

    def test_failed_reap_cleanup_and_monitor_never_become_expected_control(self):
        variations = ({'group':'unverified'}, {'cleanup':'owned-process-cleanup-timeout'},
                      {'monitor':{'operation':'unavailable'}}, {'reaped':2})
        for values in variations:
            with self.subTest(values=values):
                self.case_runner.records.clear()
                self.case_runner.failure = None
                if (self.task / 'mozilla-case').exists():
                    import shutil
                    shutil.rmtree(self.task / 'mozilla-case')
                self.assertFalse(self.run_case(negative=True, **values)['expectationSatisfied'])

    def test_unexpected_exception_after_zero_exit_does_not_count_as_positive(self):
        value = self.run_case(unexpected_error=RuntimeError('PRIVATE-SENTINEL'))
        self.assertFalse(value['expectationSatisfied'])
        self.assertEqual(value['caughtCode'], 'RuntimeError')
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(value))

    def test_missing_debug_marker_is_rejected_even_when_harness_passes(self):
        value = self.run_case(debug=False)
        altered = copy.deepcopy(value)
        altered['harness'] = smoke.receipt(self.task / 'mozilla-case/native-private/harness.raw.jsonl',
                                           [self.test.name], debug_marker=smoke.DEBUG_MARKER)
        self.assertFalse(smoke.case_satisfied(altered, 0))

    def test_retry_key_is_removed_from_child_copy_for_every_automation_value(self):
        for value in ('1', '0', '', None):
            env = {'PATH':'unchanged'}
            if value is not None:
                env['MOZ_AUTOMATION'] = value
            before = dict(env)
            child = smoke.child_environment(env)
            self.assertEqual(env, before)
            self.assertNotIn('MOZ_AUTOMATION', child)
            self.assertNotIn('MOZHTTP2_PORT', child)
            self.assertEqual(child['MOZ_ASSUME_NODE_RUNNING'], '1')

    def test_external_network_server_environment_is_rejected(self):
        for name in ('MOZHTTP2_PORT', 'MOZNODE_EXEC_PORT', 'MOZ_ASSUME_NODE_RUNNING'):
            with self.subTest(name=name), self.assertRaisesRegex(Failure, 'mozilla-smoke-network-environment'):
                smoke.child_environment({name:'1'})

    def test_harness_command_preserves_real_manifest_and_explicit_single_file(self):
        args = smoke.command(self.task, self.binary, self.manifest, self.test, self.task/'raw.jsonl')
        self.assertEqual(args[1:3], ['mach','xpcshell-test'])
        self.assertIn('--verbose', args)
        self.assertIn('--sequential', args)
        self.assertEqual(args[args.index('--threads')+1], '1')
        self.assertEqual(args[args.index('--manifest')+1], str(self.manifest))
        self.assertEqual(args[-1], str(self.test))
        for forbidden in ('--dump-tests','--verify','--run-failures','--timeout-as-pass',
                          '--crash-as-pass','--retry','--head'):
            self.assertNotIn(forbidden, args)

    def test_unavailable_process_record_cannot_establish_expected_control(self):
        evidence = smoke.process_evidence(self.case_runner)
        self.assertFalse(evidence['available'])
        for name in ('exitCode','exactReaped','groupGone','cleanupClean','monitorFailureAbsent'):
            self.assertIsNone(evidence[name])

    def test_setup_failure_keeps_callers_attached_case_and_unknown_process(self):
        attached = {}
        with mock.patch.object(Path, 'mkdir', side_effect=PermissionError('PRIVATE-SENTINEL')):
            result = smoke.run_case('setup', self.task, self.binary, self.manifest, self.test,
                self.task, self.parent, self.env, 335, value=attached)
        self.assertIs(result, attached)
        self.assertEqual(result['caseError'], 'PermissionError')
        self.assertEqual(result['caseFailurePhase'], 'setup')
        self.assertFalse(result['process']['available'])
        self.assertIsNone(result['process']['exitCode'])
        self.assertEqual(result['evidenceWriteFailure'], 'case-directory-unavailable')
        self.assertFalse(result['expectationSatisfied'])
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(result))

    def test_monitor_stop_and_write_failure_keep_original_failure_and_stop_details(self):
        stopped = {'stage':'mozilla-xpcshell', 'sampleComplete':False,
            'unknownKinds':{'inventory-timeout':1}, 'rssMiB':None,
            'queryFailure':{'operation':'kern-proc-pgrp-inventory', 'errorType':'TimeoutExpired',
                            'timeoutSeconds':3, 'elapsedSeconds':3.01, 'private':'PRIVATE-SENTINEL'}}
        with mock.patch.object(smoke, 'write_json', side_effect=OSError('PRIVATE-SENTINEL')):
            result = self.run_case(negative=True, monitor=stopped)
        self.assertFalse(result['expectationSatisfied'])
        self.assertEqual(result['runnerFailure']['code'], 'nonzero-exit')
        self.assertEqual(result['process']['exitCode'], 1)
        self.assertTrue(result['process']['exactReaped'])
        self.assertTrue(result['process']['groupGone'])
        self.assertTrue(result['monitorFailureObserved'])
        self.assertEqual(result['monitorFailure']['queryFailure']['timeoutSeconds'], 3)
        self.assertIsNone(result['monitorFailure']['rssMiB'])
        self.assertEqual(result['evidenceWriteFailure'], 'OSError')
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(result))

    def test_recent_sixteen_monitor_samples_and_separate_stop_survive_later_samples(self):
        def diagnostic(index):
            return {'stage':'mozilla-xpcshell', 'elapsedSeconds':index, 'sampleComplete':True,
                'sampleAttempts':2, 'unknownKinds':{}, 'identityTag':'PRIVATE-SENTINEL',
                'partialSamples':[{'unknownKinds':{'permission':1},
                    'formatExamples':[{'kind':'permission', 'pidTag':'PRIVATE-SENTINEL',
                        'taskQuery':{'bytes':0,'expectedBytes':232,'errno':1},
                        'bsdQuery':{'bytes':0,'expectedBytes':136,'errno':1}}]}]}
        self.case_runner.monitor_diagnostics = [diagnostic(index) for index in range(20)]
        stopped = diagnostic(2)
        value = self.run_case(negative=True, monitor=stopped)
        self.assertEqual(len(value['monitorDiagnostics']), 16)
        self.assertEqual(value['monitorDiagnostics'][0]['elapsedSeconds'], 4)
        self.assertEqual(value['monitorFailure']['elapsedSeconds'], 2)
        query = value['monitorDiagnostics'][0]['partialSamples'][0]['queryExamples'][0]
        self.assertEqual(query['taskQuery'], {'bytes':0,'expectedBytes':232,'errno':1})
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(value))
        private = json.loads((self.task/'mozilla-case/receipt.json').read_text())
        self.assertIn('PRIVATE-SENTINEL', json.dumps(private['privateEvidence']))

    def test_receipt_reader_exception_preserves_child_cleanup_and_failure(self):
        with mock.patch.object(smoke, 'receipt', side_effect=RuntimeError('PRIVATE-SENTINEL')):
            value = self.run_case(negative=True)
        self.assertFalse(value['expectationSatisfied'])
        self.assertEqual(value['caseError'], 'RuntimeError')
        self.assertEqual(value['caseFailurePhase'], 'harness-receipt')
        self.assertEqual(value['runnerFailure']['code'], 'nonzero-exit')
        self.assertTrue(value['process']['exactReaped'])
        self.assertTrue(value['process']['groupGone'])
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(value))

    def test_expected_negative_with_incomplete_cleanup_retains_cleanup_failure_code(self):
        value = self.run_case(negative=True, cleanup='owned-process-cleanup-timeout', group='unverified')
        self.assertFalse(value['expectationSatisfied'])
        self.assertEqual(value['stages'][0]['cleanupFailureCode'], 'owned-process-cleanup-timeout')
        self.assertTrue(value['stages'][0]['cleanupFailurePresent'])
        self.assertFalse(value['process']['groupGone'])

    def test_preexisting_parent_failure_is_never_cleared_by_expected_negative(self):
        original = {'stage':'earlier','code':'earlier-failure'}
        self.parent.failure = original
        value = self.run_case(negative=True)
        self.assertTrue(value['expectationSatisfied'])
        self.assertIs(self.parent.failure, original)

    def test_existing_case_receipt_is_preserved_when_new_setup_is_rejected(self):
        directory = self.task / 'mozilla-existing'
        directory.mkdir()
        path = directory / 'receipt.json'
        path.write_text('original')
        value = smoke.run_case('existing', self.task, self.binary, self.manifest, self.test,
                              self.task, self.parent, self.env, 335)
        self.assertEqual(value['caseError'], 'FileExistsError')
        self.assertFalse(value['expectationSatisfied'])
        self.assertEqual(path.read_text(), 'original')

    def test_evidence_projection_failure_retains_child_first_failure_and_private_raw_details(self):
        stopped = {'stage':'mozilla-xpcshell', 'sampleComplete':False,
                   'unknownKinds':{'permission':1}, 'private':'PRIVATE-SENTINEL'}
        with mock.patch.object(smoke, 'monitor_evidence', side_effect=ValueError('PRIVATE-SENTINEL')):
            value = self.run_case(negative=True, monitor=stopped)
        self.assertFalse(value['expectationSatisfied'])
        self.assertEqual(value['evidenceCollectionFailure'], 'ValueError')
        self.assertEqual(value['runnerFailure']['code'], 'nonzero-exit')
        self.assertTrue(value['monitorFailureObserved'])
        self.assertTrue(value['process']['exactReaped'])
        private = json.loads((self.task/'mozilla-case/receipt.json').read_text())
        self.assertEqual(private['privateEvidence']['monitorFailure'], stopped)
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(value))

    def test_unknown_child_failure_code_is_private_and_cannot_become_public_or_expected(self):
        value = self.run_case(unexpected_error=Failure('PRIVATE-SENTINEL'))
        self.assertFalse(value['expectationSatisfied'])
        self.assertEqual(value['caughtCode'], 'runner-failure')
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(value))
        private = json.loads((self.task/'mozilla-case/receipt.json').read_text())
        self.assertEqual(private['privateErrors']['launch']['message'], 'PRIVATE-SENTINEL')

    def test_unknown_setup_failure_code_is_private_and_keeps_setup_phase(self):
        with mock.patch.object(smoke, 'child_environment', side_effect=Failure('PRIVATE-SENTINEL')):
            value = smoke.run_case('private', self.task, self.binary, self.manifest, self.test,
                                  self.task, self.parent, self.env, 335)
        self.assertFalse(value['expectationSatisfied'])
        self.assertEqual(value['caseFailurePhase'], 'environment')
        self.assertEqual(value['caseError'], 'runner-failure')
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(value))

    def test_unknown_collection_and_writer_exception_names_are_never_public(self):
        private_error = type('PRIVATE_SENTINEL_Exception', (Exception,), {})
        for target in ('monitor_evidence', 'write_json'):
            with self.subTest(target=target):
                self.case_runner.records.clear()
                self.case_runner.failure = None
                if (self.task/'mozilla-case').exists():
                    import shutil
                    shutil.rmtree(self.task/'mozilla-case')
                with mock.patch.object(smoke, target, side_effect=private_error('PRIVATE-SENTINEL')):
                    value = self.run_case(negative=True, monitor={'sampleComplete':False})
                self.assertFalse(value['expectationSatisfied'])
                self.assertEqual(value['runnerFailure']['code'], 'nonzero-exit')
                field = 'evidenceCollectionFailure' if target == 'monitor_evidence' else 'evidenceWriteFailure'
                self.assertEqual(value[field], 'exception')
                self.assertNotIn('PRIVATE', json.dumps(value))


if __name__ == '__main__':
    unittest.main()

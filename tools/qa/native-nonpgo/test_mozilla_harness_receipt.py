# SPDX-License-Identifier: MPL-2.0
"""Fixtures follow Runtime 7dd619742b25567e479eeda97ae071d10542ad0b:
runxpcshelltests.py, head.js, StructuredLog.sys.mjs and mozlog. In verbose mode
Assert emits test_status before test_end; equal expected fields are omitted.
"""
import copy
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import mozilla_harness_receipt as reader


class MozillaHarnessReceiptTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix='mozilla-harness-receipt-')
        self.addCleanup(directory.cleanup)
        self.path = Path(directory.name)/'PRIVATE-SENTINEL.raw.jsonl'
        self.test = 'xpcom/tests/unit/test_PRIVATE_SENTINEL.js'
        self.debug = 'FLOORP_QA_PRIVATE-SENTINEL_DEBUG'
        self.failure = 'FLOORP_QA_PRIVATE-SENTINEL_EXPECTED_FAILURE'
        self.rows = [self.row('suite_start', tests={'xpcom/tests/unit/xpcshell.toml':[self.test]},
                              name='xpcshell'),
                     self.row('group_start', name='sequential'),
                     self.row('test_start', test=self.test, group='xpcom/tests/unit/xpcshell.toml'),
                     self.row('log', level='INFO', message=self.debug, test=self.test),
                     self.row('test_status', test=self.test, subtest='test_version', status='PASS',
                              message='[test_version : 12] PRIVATE-SENTINEL assertion'),
                     self.row('test_end', test=self.test, status='PASS', message='xpcshell return code: 0'),
                     self.row('group_end', name='sequential'), self.row('suite_end')]

    def row(self, action, **values):
        return {'action':action, 'time':1802923200000, 'thread':'MainThread',
                'pid':90001, 'source':'xpcshell', **values}

    def write(self, rows=None):
        body = ''.join(json.dumps(row)+'\n' for row in (self.rows if rows is None else rows)).encode()
        self.path.write_bytes(body)
        return body

    def read(self, *, mode='positive', debug_marker=None, failure_marker=None, expected=None):
        return reader.receipt(self.path, [self.test] if expected is None else expected, mode=mode,
                              debug_marker=debug_marker, failure_marker=failure_marker)

    def negative(self):
        self.rows[4].update(subtest=self.failure, status='FAIL', expected='PASS',
                            message='['+self.failure+' : 12] false == true PRIVATE-SENTINEL')
        self.rows[5].update(status='FAIL', expected='PASS', message='xpcshell return code: 0')

    def assert_unavailable(self, result, reason=None):
        self.assertEqual(result['status'], 'unavailable')
        self.assertFalse(result['expectationSatisfied'])
        self.assertFalse(result['completeLog'])
        self.assertIsNone(result['counts'])
        self.assertEqual(result['markers'], {'debug':None, 'failure':None})
        if reason is not None:
            self.assertEqual(result['reason'], 'mozilla-harness-'+reason)
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(result))

    def test_realistic_current_xpcshell_positive_has_full_hash_and_only_safe_aggregates(self):
        self.rows.insert(3, self.row('process_output', process='90001', data='PRIVATE-SENTINEL output',
                                     command='/PRIVATE-SENTINEL/xpcshell'))
        self.rows.insert(3, self.row('process_start', process='90001', command='/PRIVATE-SENTINEL/xpcshell'))
        self.rows.insert(-2, self.row('process_exit', process='90001', exitcode=0))
        self.rows[0]['run_info'] = {'debug':True, 'private':'/PRIVATE-SENTINEL'}
        raw = self.write()
        result = self.read(debug_marker=self.debug)
        self.assertEqual(result['status'], 'passed')
        self.assertTrue(result['expectationSatisfied'])
        self.assertTrue(result['completeLog'])
        self.assertEqual(result['bytes'], len(raw))
        self.assertEqual(result['sha256'], hashlib.sha256(raw).hexdigest())
        self.assertEqual(result['counts']['testEndStatuses']['PASS'], 1)
        self.assertEqual(result['counts']['subtestStatuses']['PASS'], 1)
        self.assertEqual(result['markers'], {'debug':True, 'failure':None})
        self.assertFalse(result['nativeQualification'])
        self.assertFalse(result['buildQualification'])
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(result))
        for value in (self.test, str(self.path), self.debug, '90001', 'MainThread'):
            self.assertNotIn(value, json.dumps(result))

    def test_multiple_expected_tests_and_groups_are_exact_and_each_runs_once(self):
        second = 'xpcom/tests/unit/test_second.js'
        rows = copy.deepcopy(self.rows)
        rows[0]['tests']['other-manifest'] = [second]
        rows[-2:-2] = [self.row('test_start', test=second),
                        self.row('test_status', test=second, subtest='same_task', status='PASS'),
                        self.row('test_end', test=second, status='PASS', expected='PASS')]
        self.write(rows)
        result = self.read(expected=(self.test, second))
        self.assertEqual(result['status'], 'passed')
        self.assertEqual(result['counts']['expectedTests'], 2)
        self.assertEqual(result['counts']['testStarts'], 2)
        self.assertEqual(result['counts']['testEnds'], 2)

    def test_upstream_xpcshell_without_subtest_assertions_can_complete_pass(self):
        rows = self.rows[:4]+self.rows[5:]
        self.write(rows)
        result = self.read()
        self.assertEqual(result['status'], 'passed')
        self.assertEqual(result['counts']['subtests'], 0)
        self.assertEqual(result['counts']['testEndStatuses']['PASS'], 1)

    def test_head_info_string_marker_includes_literal_quotes(self):
        rows = copy.deepcopy(self.rows)
        rows[3]['message'] = '"'+self.debug+'"'
        self.write(rows)
        result = self.read(debug_marker='"'+self.debug+'"')
        self.assertTrue(result['expectationSatisfied'])
        self.assertIs(result['markers']['debug'], True)

    def assert_debug_marker_ignored(self, rows, expected=None):
        self.write(rows)
        result = self.read(debug_marker=self.debug, expected=expected)
        self.assertEqual(result['status'], 'failed')
        self.assertFalse(result['expectationSatisfied'])
        self.assertTrue(result['completeLog'])
        self.assertIs(result['markers']['debug'], False)
        self.assertEqual(result['reason'], 'mozilla-harness-debug-marker-missing')

    def test_debug_marker_before_suite_start_is_ignored(self):
        rows = copy.deepcopy(self.rows)
        rows[3]['message'] = 'ordinary log'
        rows.insert(0, self.row('log', level='INFO', message=self.debug))
        self.assert_debug_marker_ignored(rows)

    def test_debug_marker_after_test_end_is_ignored(self):
        rows = copy.deepcopy(self.rows)
        rows[3]['message'] = 'ordinary log'
        rows.insert(6, self.row('log', level='INFO', message=self.debug, test=self.test))
        self.assert_debug_marker_ignored(rows)

    def test_debug_marker_after_suite_end_is_ignored(self):
        rows = copy.deepcopy(self.rows)
        rows[3]['message'] = 'ordinary log'
        rows.append(self.row('log', level='INFO', message=self.debug))
        self.assert_debug_marker_ignored(rows)

    def test_debug_marker_with_wrong_test_id_is_ignored(self):
        rows = copy.deepcopy(self.rows)
        rows[3]['test'] = 'PRIVATE-SENTINEL-unselected.js'
        self.assert_debug_marker_ignored(rows)

    def test_head_info_without_test_field_matches_the_unique_active_expected_test(self):
        rows = copy.deepcopy(self.rows)
        rows[3].pop('test')
        self.write(rows)
        result = self.read(debug_marker=self.debug)
        self.assertEqual(result['status'], 'passed')
        self.assertTrue(result['expectationSatisfied'])
        self.assertIs(result['markers']['debug'], True)

    def test_debug_marker_with_multiple_active_tests_is_ignored(self):
        rows = copy.deepcopy(self.rows)
        second = 'xpcom/tests/unit/test_second.js'
        rows[0]['tests']['xpcom/tests/unit/xpcshell.toml'].append(second)
        rows.insert(3, self.row('test_start', test=second))
        rows.insert(7, self.row('test_end', test=second, status='PASS'))
        self.assert_debug_marker_ignored(rows, [self.test, second])

    def test_current_xpcshell_assert_failure_is_detected_without_masking_failed_status(self):
        self.negative()
        self.write()
        result = self.read(mode='negative-control', debug_marker=self.debug, failure_marker=self.failure)
        self.assertEqual(result['status'], 'failed')
        self.assertTrue(result['expectationSatisfied'])
        self.assertEqual(result['reason'], 'mozilla-harness-negative-control-detected')
        self.assertEqual(result['counts']['testEndStatuses']['FAIL'], 1)
        self.assertEqual(result['counts']['subtestStatuses']['FAIL'], 1)
        self.assertEqual(result['counts']['unexpectedSubtests'], 1)
        self.assertEqual(result['counts']['unexpectedTestEnds'], 1)
        self.assertEqual(result['markers'], {'debug':True, 'failure':True})
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(result))

    def test_failure_marker_may_match_exact_message_instead_of_task_name(self):
        self.negative()
        self.rows[4].update(subtest='run_test', message=self.failure)
        self.write()
        result = self.read(mode='negative-control', failure_marker=self.failure)
        self.assertTrue(result['expectationSatisfied'])

    def test_positive_rejects_failures_skips_unexpected_passes_and_known_failures(self):
        for changes in ({'status':'FAIL','expected':'PASS'}, {'status':'SKIP'},
                        {'status':'PASS','expected':'FAIL'}, {'status':'FAIL','expected':'FAIL'},
                        {'status':'ERROR'}, {'status':'TIMEOUT'}, {'status':'ASSERT'}, {'status':'NOTRUN'}):
            rows = copy.deepcopy(self.rows)
            rows[4].update(changes)
            self.write(rows)
            result = self.read()
            self.assertEqual(result['status'], 'failed')
            self.assertFalse(result['expectationSatisfied'])
            self.assertTrue(result['completeLog'])
            self.assertEqual(result['counts']['subtestStatuses'][changes['status']], 1)

    def test_negative_requires_one_unexpected_status_and_one_unexpected_end(self):
        self.negative()
        cases = []
        for index, changes in ((4, {'status':'PASS'}), (4, {'expected':'FAIL'}),
                               (5, {'expected':'FAIL'}), (5, {'expected':'ERROR'}), (5, {'status':'PASS'}),
                               (4, {'subtest':'other', 'message':self.failure+' suffix'})):
            rows = copy.deepcopy(self.rows)
            rows[index].update(changes)
            cases.append(rows)
        rows = copy.deepcopy(self.rows)
        rows.insert(5, copy.deepcopy(rows[4]))
        cases.append(rows)
        for rows in cases:
            self.write(rows)
            result = self.read(mode='negative-control', failure_marker=self.failure)
            self.assertEqual(result['status'], 'failed')
            self.assertFalse(result['expectationSatisfied'])
            self.assertEqual(result['reason'], 'mozilla-harness-negative-control-mismatch')

    def test_negative_without_any_failure_does_not_turn_success_into_a_valid_control(self):
        self.write()
        result = self.read(mode='negative-control', failure_marker=self.failure)
        self.assertEqual(result['status'], 'failed')
        self.assertFalse(result['expectationSatisfied'])
        self.assertEqual(result['counts']['testEndStatuses']['PASS'], 1)
        self.assertEqual(result['counts']['testEndStatuses']['FAIL'], 0)

    def test_debug_marker_requires_exact_info_message_not_dump_warning_or_substring(self):
        for row in (self.row('log', level='INFO', message=self.debug+' suffix'),
                    self.row('log', level='WARNING', message=self.debug),
                    self.row('process_output', process='90001', data=self.debug),
                    self.row('log', level='INFO', message='"'+self.debug+'"')):
            rows = copy.deepcopy(self.rows)
            rows[3] = row
            self.write(rows)
            result = self.read(debug_marker=self.debug)
            self.assertFalse(result['expectationSatisfied'])
            self.assertFalse(result['markers']['debug'])
            self.assertEqual(result['reason'], 'mozilla-harness-debug-marker-missing')

    def test_other_errors_crashes_timeouts_nonzero_process_and_assertions_reject_both_modes(self):
        extras = [self.row('log', level='ERROR', message='PRIVATE-SENTINEL'),
                  self.row('log', level='INFO', message='TEST-UNEXPECTED-FAIL PRIVATE-SENTINEL'),
                  self.row('crash', minidump_path='/PRIVATE-SENTINEL'),
                  self.row('shutdown_failure', message='PRIVATE-SENTINEL'),
                  self.row('process_output', data='PROCESS-CRASH PRIVATE-SENTINEL'),
                  self.row('process_output', data='TEST-UNEXPECTED-TIMEOUT PRIVATE-SENTINEL'),
                  self.row('process_exit', process='90001', exitcode=1),
                  self.row('assertion_count', test=self.test, count=2, min_expected=0, max_expected=0)]
        for mode in ('positive', 'negative-control'):
            if mode == 'negative-control':
                self.negative()
            for extra in extras:
                rows = copy.deepcopy(self.rows)
                rows.insert(-2, extra)
                self.write(rows)
                result = self.read(mode=mode, failure_marker=self.failure if mode != 'positive' else None)
                self.assertEqual(result['status'], 'failed')
                self.assertFalse(result['expectationSatisfied'])
                self.assertTrue(result['completeLog'])

    def test_unselected_duplicate_listings_wrong_ids_and_flat_test_lists_are_rejected(self):
        for tests, reason in (({'group':[self.test,self.test]}, 'selected-tests'),
                              ({'group':['PRIVATE-SENTINEL']}, 'selected-tests'),
                              ([self.test], 'suite-tests'), ({'group':[True]}, 'suite-tests')):
            rows = copy.deepcopy(self.rows)
            rows[0]['tests'] = tests
            self.write(rows)
            self.assert_unavailable(self.read(), reason)
        rows = copy.deepcopy(self.rows)
        rows[2]['test'] = 'PRIVATE-SENTINEL'
        self.write(rows)
        self.assert_unavailable(self.read(), 'test-order')

    def test_partial_suites_lifecycle_duplicates_retries_and_wrong_order_fail_closed(self):
        cases = [self.rows[:-1], self.rows[1:], self.rows[:2]+self.rows[3:],
                 self.rows[:2]+[self.rows[0]]+self.rows[2:],
                 self.rows[:3]+[self.rows[2]]+self.rows[3:],
                 self.rows[:6]+[self.rows[5]]+self.rows[6:],
                 self.rows[:6]+[self.rows[2],self.rows[5]]+self.rows[6:],
                 self.rows[:4]+[self.rows[5],self.rows[4]]+self.rows[6:],
                 self.rows+[self.rows[-1]]]
        for rows in cases:
            self.write(rows)
            self.assert_unavailable(self.read())

    def test_nonverbose_xpcshell_buffer_replay_after_test_end_is_ambiguous(self):
        self.negative()
        rows = self.rows[:4]+[self.rows[5], self.row('group_start',name='replaying PRIVATE-SENTINEL'),
                               self.rows[4], self.row('group_end',name='replaying PRIVATE-SENTINEL')]+self.rows[6:]
        self.write(rows)
        self.assert_unavailable(self.read(mode='negative-control', failure_marker=self.failure), 'test-order')

    def test_boolean_status_expected_time_and_unknown_measurements_never_mean_zero(self):
        for index, changes in ((4, {'status':True}), (4, {'expected':False}), (2, {'time':True}),
                               (4, {'subtest':None}), (4, {'message':False}),
                               (4, {'known_intermittent':['FAIL']})):
            rows = copy.deepcopy(self.rows)
            rows[index].update(changes)
            self.write(rows)
            self.assert_unavailable(self.read())
        for extra in (self.row('process_exit', exitcode=False),
                      self.row('assertion_count', count=False, min_expected=0, max_expected=0),
                      self.row('mozleak_total', bytes=None, threshold=0),
                      self.row('PRIVATE-SENTINEL', count=0)):
            rows = copy.deepcopy(self.rows)
            rows.insert(-2, extra)
            self.write(rows)
            self.assert_unavailable(self.read())

    def test_malformed_json_duplicate_keys_invalid_utf8_and_partial_final_line_reject(self):
        valid = self.write()
        for raw, reason in ((b'PRIVATE-SENTINEL\n','json'), (b'{"action":"log","action":"log"}\n','json'),
                            (b'{"action":"log","time":NaN}\n','json'), (b'\xff\n','utf8'),
                            (valid[:-1],'partial-log'), (valid+b'\n','line-size'), (b'[]\n','event-shape')):
            self.path.write_bytes(raw)
            result = self.read()
            self.assert_unavailable(result, reason)
            self.assertEqual(result['sha256'], hashlib.sha256(raw).hexdigest())

    def test_empty_missing_symlink_directory_fifo_and_foreign_owner_are_unavailable(self):
        self.assert_unavailable(self.read(), 'file-read')
        self.path.write_bytes(b'')
        self.assert_unavailable(self.read(), 'file-size')
        self.path.unlink()
        self.path.symlink_to(self.path.parent/'PRIVATE-SENTINEL-target')
        self.assert_unavailable(self.read(), 'file-kind')
        self.path.unlink()
        self.path.mkdir()
        self.assert_unavailable(self.read(), 'file-kind')
        self.path.rmdir()
        if hasattr(os, 'mkfifo'):
            os.mkfifo(self.path)
            self.assert_unavailable(self.read(), 'file-kind')
            self.path.unlink()
        self.write()
        with mock.patch.object(reader.os, 'geteuid', return_value=os.geteuid()+1):
            self.assert_unavailable(self.read(), 'file-kind')

    def test_file_growth_or_path_replacement_is_unavailable_and_descriptor_is_closed(self):
        self.write()
        read = os.read
        changed = False
        descriptors = []
        def changing(descriptor, length):
            nonlocal changed
            descriptors.append(descriptor)
            block = read(descriptor, length)
            if not changed:
                changed = True
                with self.path.open('ab') as output:
                    output.write(b'PRIVATE-SENTINEL\n')
            return block
        with mock.patch.object(reader.os, 'read', side_effect=changing):
            self.assert_unavailable(self.read(), 'file-changed')
        with self.assertRaises(OSError):
            os.fstat(descriptors[0])
        self.write()
        changed = False
        def replacing(descriptor, length):
            nonlocal changed
            block = read(descriptor, length)
            if not changed:
                changed = True
                self.path.rename(self.path.with_suffix('.old'))
                self.write()
            return block
        with mock.patch.object(reader.os, 'read', side_effect=replacing):
            self.assert_unavailable(self.read(), 'file-changed')

    def test_file_line_and_record_caps_are_bounded_without_partial_measurements(self):
        self.write()
        with mock.patch.object(reader, 'MAX_BYTES', 10):
            self.assert_unavailable(self.read(), 'file-size')
        with mock.patch.object(reader, 'MAX_LINE_BYTES', 10):
            self.assert_unavailable(self.read(), 'line-size')
        with mock.patch.object(reader, 'MAX_RECORDS', 2):
            self.assert_unavailable(self.read(), 'record-cap')

    def test_invalid_inputs_do_not_open_a_log_or_publish_input_values(self):
        cases = [(None, {}), ([], {}), ([True], {}), ([self.test,self.test], {}),
                 ([self.test], {'mode':'PRIVATE-SENTINEL'}),
                 ([self.test], {'debug_marker':True}), ([self.test], {'failure_marker':'PRIVATE-SENTINEL'}),
                 ([self.test], {'mode':'negative-control'}),
                 ([self.test,'second'], {'mode':'negative-control','failure_marker':self.failure})]
        with mock.patch.object(reader.os, 'open') as opened:
            for expected, kwargs in cases:
                self.assert_unavailable(reader.receipt(self.path, expected, **kwargs), 'input')
        opened.assert_not_called()

    def test_input_records_expected_ids_and_environment_remain_unchanged(self):
        self.negative()
        raw = self.write()
        expected = [self.test]
        saved_expected, environment = list(expected), dict(os.environ)
        result = reader.receipt(self.path, expected, mode='negative-control',
                                debug_marker=self.debug, failure_marker=self.failure)
        self.assertTrue(result['expectationSatisfied'])
        self.assertEqual(expected, saved_expected)
        self.assertEqual(dict(os.environ), environment)
        self.assertEqual(self.path.read_bytes(), raw)


if __name__ == '__main__':
    unittest.main()

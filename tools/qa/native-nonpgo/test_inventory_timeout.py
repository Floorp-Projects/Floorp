# SPDX-License-Identifier: MPL-2.0
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

from support import Failure, Runner


class InventoryTimeoutTests(unittest.TestCase):
    def timeout(self):
        return subprocess.TimeoutExpired('PRIVATE_COMMAND', 3,
                                        output=b'PRIVATE_PARTIAL_INVENTORY', stderr=b'PRIVATE_STDERR')

    def complete(self):
        return {'sampleComplete': True, 'rssMiB': 4, 'cpuSeconds': 1,
                'rssLowerBoundMiB': 4, 'cpuLowerBoundSeconds': 1,
                'processes': 1, 'states': ['R'], '_pids': (901,),
                'unknownKinds': {}, 'formatExamples': []}

    def test_timeout_then_complete_uses_existing_retry_on_both_platform_paths(self):
        for platform in ('darwin', 'linux'):
            with self.subTest(platform=platform), \
                    mock.patch('support.sys.platform', platform), \
                    mock.patch('support.subprocess.run', side_effect=[self.timeout(), mock.Mock(stdout='COMPLETE')]) as query, \
                    mock.patch('darwin_activity.probe') as sensor, \
                    mock.patch.object(Runner, 'parse_group_activity', return_value=self.complete()) as parser, \
                    mock.patch('support.time.sleep') as pause:
                sensor.return_value.group.return_value = self.complete()
                value = Runner.group_activity(900)
                self.assertTrue(value['sampleComplete'])
                self.assertEqual(value['sampleAttempts'], 2)
                self.assertEqual(value['rssMiB'], 4)
                partial, = value['partialSamples']
                self.assertEqual(partial['unknownKinds'], {'inventory-timeout': 1})
                self.assertIsNone(partial['knownRSSLowerBoundMiB'])
                self.assertIsNone(partial['knownCPULowerBoundSeconds'])
                failure = partial['queryFailure']
                self.assertEqual(failure['operation'], 'process-group-inventory')
                self.assertEqual(failure['errorType'], 'TimeoutExpired')
                self.assertEqual(failure['timeoutSeconds'], 3)
                self.assertEqual(query.call_count, 2)
                self.assertTrue(all(call.kwargs['timeout'] == 3 for call in query.call_args_list))
                pause.assert_called_once_with(0.25)
                if platform == 'darwin':
                    sensor.return_value.group.assert_called_once_with('COMPLETE', 900)
                    parser.assert_not_called()
                else:
                    parser.assert_called_once_with('COMPLETE', 900)
                    sensor.assert_not_called()
                self.assertNotIn('PRIVATE', json.dumps(value))

    def test_three_timeouts_are_unknown_and_exhaust_exact_sample_budget(self):
        with mock.patch('support.subprocess.run', side_effect=self.timeout()) as query, \
                mock.patch('support.time.sleep') as pause, \
                mock.patch('darwin_activity.probe') as sensor:
            value = Runner.group_activity(900)
        self.assertFalse(value['sampleComplete'])
        self.assertEqual(value['sampleAttempts'], 3)
        self.assertEqual(len(value['partialSamples']), 3)
        self.assertEqual(query.call_count, 3)
        self.assertEqual(pause.call_args_list, [mock.call(0.25), mock.call(0.25)])
        sensor.assert_not_called()
        for field in ('rssMiB', 'cpuSeconds', 'rssLowerBoundMiB', 'cpuLowerBoundSeconds', 'processes'):
            self.assertIsNone(value[field])
        self.assertEqual(value['states'], ['?'])
        self.assertNotIn('PRIVATE', json.dumps(value))

    def test_query_non_timeout_errors_still_propagate_without_retry(self):
        for error in (PermissionError(13, 'PRIVATE'), subprocess.CalledProcessError(1, 'PRIVATE')):
            with self.subTest(error=type(error).__name__), \
                    mock.patch('support.subprocess.run', side_effect=error) as query, \
                    mock.patch('support.time.sleep') as pause:
                with self.assertRaises(type(error)):
                    Runner.group_activity(900)
                self.assertEqual(query.call_count, 1)
                pause.assert_not_called()

    def test_partial_counter_sample_timeout_and_recovery_preserve_both_failures(self):
        partial = {**self.complete(), 'sampleComplete': False, 'rssMiB': None,
                   'cpuSeconds': None, 'unknownKinds': {'short-task-info': 1}}
        with mock.patch('support.sys.platform', 'darwin'), \
                mock.patch('support.subprocess.run', side_effect=[mock.Mock(stdout='FIRST'), self.timeout(), mock.Mock(stdout='LAST')]) as query, \
                mock.patch('darwin_activity.probe') as sensor, \
                mock.patch('support.time.sleep') as pause:
            sensor.return_value.group.side_effect = [partial, self.complete()]
            value = Runner.group_activity(900)
        self.assertTrue(value['sampleComplete'])
        self.assertEqual(value['sampleAttempts'], 3)
        self.assertEqual([x['unknownKinds'] for x in value['partialSamples']],
                         [{'short-task-info': 1}, {'inventory-timeout': 1}])
        self.assertEqual(query.call_count, 3)
        self.assertEqual(sensor.return_value.group.call_count, 2)
        self.assertEqual(pause.call_count, 2)

    def test_counter_probe_exceptions_are_not_misclassified_as_query_timeouts(self):
        for error in (self.timeout(), KeyboardInterrupt()):
            with self.subTest(error=type(error).__name__), \
                    mock.patch('support.sys.platform', 'darwin'), \
                    mock.patch('support.subprocess.run', return_value=mock.Mock(stdout='COMPLETE')) as query, \
                    mock.patch('darwin_activity.probe') as sensor, \
                    mock.patch('support.time.sleep') as pause:
                sensor.return_value.group.side_effect = error
                with self.assertRaises(type(error)):
                    Runner.group_activity(900)
                self.assertEqual(query.call_count, 1)
                pause.assert_not_called()

    def test_real_timed_out_query_child_is_killed_and_reaped_before_resampling(self):
        original_run, original_popen = subprocess.run, subprocess.Popen
        children = []
        def launch(*args, **kwargs):
            child = original_popen(*args, **kwargs)
            children.append(child)
            return child
        def query(args, **kwargs):
            self.assertEqual(args[:2], ['/bin/ps', '-axo'])
            self.assertEqual(kwargs['timeout'], 3)
            if len(children) == 0:
                return original_run([sys.executable, '-c', 'import time; time.sleep(30)'],
                                    **{**kwargs, 'timeout': 0.05})
            self.assertIsNotNone(children[0].returncode)
            with self.assertRaises(ChildProcessError):
                os.waitpid(children[0].pid, os.WNOHANG)
            return mock.Mock(stdout='900 901 4096 0:01 R\n')
        with mock.patch('support.sys.platform', 'linux'), \
                mock.patch('support.subprocess.Popen', side_effect=launch), \
                mock.patch('support.subprocess.run', side_effect=query), \
                mock.patch('support.time.sleep'):
            value = Runner.group_activity(900)
        self.assertTrue(value['sampleComplete'])
        self.assertEqual(value['sampleAttempts'], 2)
        self.assertEqual(len(children), 1)
        self.assertIsNotNone(children[0].returncode)

    def test_persistent_timeouts_record_classification_and_cleanup_real_stage(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        with tempfile.TemporaryDirectory() as directory:
            task = Path(directory)
            runner = Runner(task, time.time(), limits)
            durable = io.StringIO()
            with mock.patch('support.subprocess.run', side_effect=self.timeout()), \
                    contextlib.redirect_stdout(durable):
                with self.assertRaisesRegex(Failure, 'monitor-unavailable'):
                    runner.run('inventory-timeout', [sys.executable, '-c', 'import time; time.sleep(30)'],
                               task, os.environ.copy(), 360)
            record = runner.records[-1]
            self.assertEqual(runner.failure['code'], 'monitor-unavailable')
            self.assertEqual(record['failurePhase'], 'group-activity')
            self.assertEqual(record['groupCleanup'], 'verified-gone')
            self.assertEqual(record['leaderReap']['status'], 'reaped')
            self.assertIsNone(record['peakRSSMiB'])
            failure = runner.monitor_failure
            self.assertEqual(failure['unknownKinds'], {'inventory-timeout': 1})
            self.assertEqual(failure['queryFailure']['operation'], 'process-group-inventory')
            self.assertEqual(failure['sampleAttempts'], 3)
            self.assertIsNone(failure['processes'])
            self.assertEqual(len(failure['partialSamples']), 3)
            self.assertEqual(json.loads((task / 'first-failure.json').read_text()), runner.failure)
            self.assertNotIn('PRIVATE', json.dumps(record) + json.dumps(failure) + durable.getvalue())
            self.assertIn('before-cleanup', durable.getvalue())
            self.assertIn('after-cleanup', durable.getvalue())

    def test_recovered_timeout_does_not_stop_real_stage(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        original_run = subprocess.run
        calls = 0
        def query(*args, **kwargs):
            nonlocal calls
            calls += 1
            if calls == 1:
                raise self.timeout()
            return original_run(*args, **kwargs)
        with tempfile.TemporaryDirectory() as directory:
            task = Path(directory)
            runner = Runner(task, time.time(), limits)
            with mock.patch('support.subprocess.run', side_effect=query), contextlib.redirect_stdout(io.StringIO()):
                runner.run('inventory-recovered', [sys.executable, '-c', 'import time; time.sleep(1)'],
                           task, os.environ.copy(), 360)
            self.assertIsNone(runner.failure)
            self.assertIsNone(runner.monitor_failure)
            self.assertEqual(runner.records[-1]['groupCleanup'], 'verified-gone')
            self.assertEqual(runner.records[-1]['leaderReap'], {'status': 'reaped', 'exitCode': 0})
            self.assertEqual(runner.monitor_diagnostics[-1]['sampleAttempts'], 2)
            self.assertEqual(runner.monitor_diagnostics[-1]['partialSamples'][0]['unknownKinds'],
                             {'inventory-timeout': 1})


if __name__ == '__main__':
    unittest.main()

# SPDX-License-Identifier: MPL-2.0
import ctypes
import contextlib
import errno
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest import mock

from failure_excerpt import excerpt
from support import Failure, Runner
import test_guards


class ESRCHRecheckTests(unittest.TestCase):
    def sensor(self, actions):
        sensor, library = test_guards.DarwinCounterGuards().sensor()
        original = library.proc_pidinfo.side_effect
        remaining = list(actions)
        def query(pid, flavor, arg, pointer, size):
            action = remaining.pop(0)
            if action == 'esrch' or action == 'permission':
                ctypes.set_errno(errno.ESRCH if action == 'esrch' else errno.EPERM)
                return 0
            value = original(pid, flavor, arg, pointer, size)
            bsd = pointer._obj.bsd if flavor == 2 else pointer._obj
            if action in ('new', 'new-zombie'):
                bsd.start_usec += 1
            if action in ('zombie', 'new-zombie'):
                bsd.status = 5
            if action == 'foreign':
                bsd.uid += 1
            if action == 'wrong-group':
                bsd.pgid += 1
            if action == 'invalid':
                bsd.start_sec = 0
            return value
        library.proc_pidinfo.side_effect = query
        return sensor, library, remaining

    def test_same_generation_recovered_counters_are_measured(self):
        sensor, library, remaining = self.sensor(['esrch', 'live', 'live'])
        value = sensor.read(42, 900)
        self.assertEqual((value['status'], value['rssMiB']), ('measured', 8))
        self.assertTrue(value['esrchRecheck']['sameGeneration'])
        self.assertEqual(value['esrchRecheck']['firstTaskQuery']['errno'], errno.ESRCH)
        self.assertEqual(library.proc_pidinfo.call_count, 3)
        self.assertEqual(remaining, [])

    def test_same_birth_zombie_and_independent_esrch_are_gone(self):
        for final in ('zombie', 'esrch'):
            with self.subTest(final=final):
                sensor, _, _ = self.sensor(['esrch', 'live', 'esrch', final])
                with mock.patch('darwin_activity.os.kill', side_effect=ProcessLookupError):
                    value = sensor.read(42, 900)
                self.assertEqual(value['status'], 'gone')
                self.assertNotIn('rssMiB', value)

    def test_new_live_group_generation_must_not_be_excluded(self):
        sensor, _, _ = self.sensor(['esrch', 'live', 'new'])
        value = sensor.group(f'900 42 1 {os.geteuid()} {os.getuid()} /private/name\n', 900)
        self.assertFalse(value['sampleComplete'])
        self.assertIsNone(value['rssMiB'])
        self.assertEqual(value['unknownKinds'], {'generation-changed-during-recheck': 1})
        self.assertEqual(value['confirmedGone'], 0)
        self.assertFalse(value['esrchExamples'][0]['sameGeneration'])

    def test_external_owned_generation_retains_replaced_semantics(self):
        sensor, _, _ = self.sensor(['esrch', 'live', 'new'])
        value = sensor.read(42, 900, {'pid': 42, 'startSec': 100, 'startUsec': 7})
        self.assertEqual((value['status'], value['reason']), ('gone', 'generation-replaced'))

    def test_new_zombie_is_not_evidence_for_internal_anchor(self):
        sensor, _, _ = self.sensor(['esrch', 'live', 'esrch', 'new-zombie'])
        value = sensor.read(42, 900)
        self.assertEqual(value['status'], 'unavailable')

    def test_permission_invalid_and_foreign_do_not_trigger_recheck(self):
        for actions, calls in [(['permission'], 1), (['esrch', 'invalid'], 2),
                               (['esrch', 'foreign'], 2), (['esrch', 'wrong-group'], 2)]:
            with self.subTest(actions=actions):
                sensor, library, _ = self.sensor(actions)
                self.assertEqual(sensor.read(42, 900)['status'], 'unavailable')
                self.assertEqual(library.proc_pidinfo.call_count, calls)

    def test_live_unknown_and_recheck_permission_stay_unknown(self):
        for actions in (['esrch', 'live', 'esrch', 'live'], ['esrch', 'live', 'permission']):
            with self.subTest(actions=actions):
                sensor, _, remaining = self.sensor(actions)
                value = sensor.read(42, 900)
                self.assertEqual(value['status'], 'unavailable')
                self.assertNotIn('rssMiB', value)
                self.assertEqual(remaining, [])

    def test_partial_task_esrch_cannot_trigger_zero_byte_recheck(self):
        sensor, library, _ = self.sensor(['esrch', 'live'])
        original = library.proc_pidinfo.side_effect
        def partial(pid, flavor, *args):
            size = original(pid, flavor, *args)
            return 231 if flavor == 2 else size
        library.proc_pidinfo.side_effect = partial
        value = sensor.read(42, 900)
        self.assertEqual(value['status'], 'unavailable')
        self.assertNotIn('esrchRecheck', value)
        self.assertEqual(library.proc_pidinfo.call_count, 2)

    def test_three_distinct_unresolved_members_keep_global_retry_bound(self):
        sensor, library, _ = self.sensor(['esrch', 'live', 'esrch', 'live'] * 3)
        rows = [mock.Mock(stdout=f'900 {pid} 1 {os.geteuid()} {os.getuid()} /private/name\n')
                for pid in (41, 42, 43)]
        with mock.patch('support.sys.platform', 'darwin'), \
                mock.patch('darwin_activity.probe', return_value=sensor), \
                mock.patch('support.subprocess.run', side_effect=rows) as inventory, \
                mock.patch('support.time.sleep') as pause:
            value = Runner.group_activity(900, bootstrap=True)
        self.assertFalse(value['sampleComplete'])
        self.assertIsNone(value['rssMiB'])
        self.assertIsNone(value['cpuSeconds'])
        self.assertEqual((inventory.call_count, library.proc_pidinfo.call_count), (3, 12))
        self.assertEqual(pause.call_args_list, [mock.call(0.25), mock.call(0.25)])
        tags = [x['esrchExamples'][0]['pidTag'] for x in value['partialSamples']]
        self.assertEqual(len(set(tags)), 3)


class FailureExcerptTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def test_shapes_preserve_diagnosis_without_arbitrary_text(self):
        path = self.root / 'compile.private.log'
        path.write_text("/private/SECRET/file.cc:3:2: error: use of undeclared identifier 'PASSWORD'\n"
                        'error[E0308]: API_TOKEN=value https://signed.invalid?sig=SECRET\n'
                        'clang: error: unable to execute command: Segmentation fault: 11\n')
        value = excerpt(path)
        encoded = json.dumps(value)
        for private in ('SECRET', 'PASSWORD', 'API_TOKEN', 'signed.invalid', '/private/', 'file.cc'):
            self.assertNotIn(private, encoded)
        self.assertEqual(value['records'][0]['messageClass'], 'undeclared-identifier')
        self.assertIn('E0308', encoded)
        self.assertIn('segmentation-fault', encoded)
        self.assertIn('unable-to-execute', encoded)

    def test_bounds_keep_first_and_last_records_and_unclassified_markers(self):
        path = self.root / 'large.private.log'
        path.write_bytes(b'PRIVATE' * 30000 + b'\n' + b'error: unknown SECRET\n' * 25)
        value = excerpt(path)
        self.assertTrue(value['tailTruncated'])
        self.assertLessEqual(value['tailBytesRead'], 131072)
        self.assertEqual(value['matchingRecords'], 25)
        self.assertEqual(len(value['records']), 16)
        self.assertTrue(value['recordsTruncated'])
        self.assertEqual(value['records'][-1]['tailLine'], 25)
        self.assertNotIn('SECRET', json.dumps(value))

    def test_missing_symlink_and_fifo_are_bounded_unavailable(self):
        missing = self.root / 'missing'
        self.assertFalse(excerpt(missing)['available'])
        link = self.root / 'link'
        link.symlink_to(missing)
        self.assertFalse(excerpt(link)['available'])
        fifo = self.root / 'fifo'
        os.mkfifo(fifo)
        self.assertFalse(excerpt(fifo)['available'])

    def test_real_monitor_stop_preserves_before_and_after_cleanup(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        runner = Runner(self.root, time.time(), limits)
        path = self.root / 'compile.private.log'
        def unavailable(_pgid):
            deadline = time.monotonic() + 3
            while path.stat().st_size == 0 and time.monotonic() < deadline:
                time.sleep(0.005)
            self.assertGreater(path.stat().st_size, 0)
            return {'sampleComplete': False, 'rssMiB': None, 'cpuSeconds': None,
                    'rssLowerBoundMiB': 0, 'cpuLowerBoundSeconds': 0,
                    'unknownKinds': {'short-task-info': 1}, 'formatExamples': []}
        script = "import time; print(\"x.cc:1:2: error: use of undeclared identifier 'SECRET'\",flush=True); time.sleep(30)"
        durable = io.StringIO()
        with mock.patch.object(runner, 'group_activity', side_effect=unavailable), contextlib.redirect_stdout(durable):
            with self.assertRaisesRegex(Failure, 'monitor-unavailable'):
                runner.run('compile', [sys.executable, '-c', script], self.root, os.environ.copy(), 360)
        record = runner.records[-1]
        self.assertEqual(record['groupCleanup'], 'verified-gone')
        self.assertEqual(record['leaderReap']['status'], 'reaped')
        self.assertEqual(runner.failure['code'], 'monitor-unavailable')
        before = record['failureEvidence']['beforeCleanup']
        after = record['failureEvidence']['afterCleanup']
        self.assertEqual([x['kind'] for x in before['records']], ['source-error'])
        self.assertIn('keyboard-interrupt', json.dumps(after))
        self.assertNotIn('SECRET', json.dumps(record))
        emitted = [json.loads(line)['failureExcerpt'] for line in durable.getvalue().splitlines()]
        self.assertEqual([x['phase'] for x in emitted], ['before-cleanup', 'after-cleanup'])
        self.assertEqual(emitted[0]['evidence'], before)
        self.assertEqual(emitted[1]['evidence'], after)
        self.assertNotIn('SECRET', durable.getvalue())

    def test_unknown_diagnostic_preserves_safe_words_and_strips_color_and_values(self):
        path = self.root / 'unknown.private.log'
        path.write_text("\x1b[31m/private/PASSWORD/x.cc:1:2: error: out-of-line definition of 'SECRET' "
                        "does not match any declaration in `PRIVATE` API_TOKEN=value\x1b[0m\n")
        result = excerpt(path)
        value = result['records'][0]['normalizedDiagnostic']
        self.assertIn('out of line definition', value)
        self.assertIn('does not match any declaration in', value)
        for private in ('PASSWORD', 'SECRET', 'PRIVATE', 'API_TOKEN', 'value', '/private'):
            self.assertNotIn(private, json.dumps(result))
        self.assertLessEqual(len(value), 240)

    def test_unreadable_diagnostics_do_not_change_primary_failure(self):
        with mock.patch('failure_excerpt.os.open', side_effect=PermissionError):
            self.assertFalse(excerpt(self.root / 'private')['available'])
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        runner = Runner(self.root, time.time(), limits)
        activity = {'sampleComplete': False, 'rssMiB': None, 'cpuSeconds': None,
                    'rssLowerBoundMiB': 0, 'cpuLowerBoundSeconds': 0}
        with mock.patch.object(runner, 'group_activity', return_value=activity), \
                mock.patch('support.excerpt', return_value={'available': False, 'errorType': 'PermissionError'}):
            with self.assertRaisesRegex(Failure, 'monitor-unavailable'):
                runner.run('unreadable', [sys.executable, '-c', 'import time; time.sleep(30)'],
                           self.root, os.environ.copy(), 360)
        self.assertEqual(runner.failure['code'], 'monitor-unavailable')
        self.assertEqual(runner.records[-1]['groupCleanup'], 'verified-gone')
        self.assertFalse(runner.records[-1]['failureEvidence']['beforeCleanup']['available'])

    def test_cleanup_only_failure_does_not_mislabel_a_postcleanup_excerpt(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        runner = Runner(self.root, time.time(), limits)
        registry = mock.Mock()
        registry.outside_group_rss.return_value = 0
        def finish():
            (self.root / 'late.private.log').write_text('error: SECRET created during cleanup\n')
            raise Failure('cleanup-late-error')
        registry.finish.side_effect = finish
        runner.native_registry = registry
        complete = {'sampleComplete': True, 'rssMiB': 1, 'cpuSeconds': 0,
                    'processes': 1, 'states': ['R'], '_pids': ()}
        with mock.patch.object(runner, 'group_activity', return_value=complete):
            with self.assertRaisesRegex(Failure, 'cleanup-late-error'):
                runner.run('late', [sys.executable, '-c', 'pass'], self.root, os.environ.copy(), 360)
        evidence = runner.records[-1]['failureEvidence']
        self.assertEqual(evidence['beforeCleanup'], {'available': False, 'reason': 'not-captured-before-cleanup'})
        self.assertEqual(evidence['afterCleanup']['records'][0]['kind'], 'unclassified-error-marker')
        self.assertNotIn('SECRET', json.dumps(evidence))

    def test_throwing_collector_cannot_replace_failure_or_stage_record(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        runner = Runner(self.root, time.time(), limits)
        activity = {'sampleComplete': False, 'rssMiB': None, 'cpuSeconds': None,
                    'rssLowerBoundMiB': 0, 'cpuLowerBoundSeconds': 0}
        with mock.patch.object(runner, 'group_activity', return_value=activity), \
                mock.patch('support.excerpt', side_effect=RuntimeError('PRIVATE SECRET')):
            with self.assertRaisesRegex(Failure, 'monitor-unavailable'):
                runner.run('throwing', [sys.executable, '-c', 'import time; time.sleep(30)'],
                           self.root, os.environ.copy(), 360)
        self.assertEqual(runner.failure['code'], 'monitor-unavailable')
        record = runner.records[-1]
        self.assertEqual(record['groupCleanup'], 'verified-gone')
        self.assertEqual(record['leaderReap']['status'], 'reaped')
        self.assertEqual(record['failurePhase'], 'group-activity')
        for name in ('beforeCleanup', 'afterCleanup'):
            self.assertEqual(record['failureEvidence'][name]['errorType'], 'diagnostic-collector-error')
        self.assertNotIn('SECRET', json.dumps(record))


if __name__ == '__main__':
    unittest.main()

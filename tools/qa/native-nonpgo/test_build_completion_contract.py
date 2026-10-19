# SPDX-License-Identifier: MPL-2.0
import contextlib
import io
import json
import multiprocessing
import os
from pathlib import Path
import tempfile
import types
import unittest
from unittest import mock

import build_completion_contract as contract


def clean():
    return {key: True for key in ('streamClosed', 'parentPipeClosed', 'childPipeClosed',
                                  'processClosed', 'workerNeverStarted')} | {'failures': []}


def profile_rows(mode):
    rows = [dict(type='meta', product='mach', version=27, preprocessedProfileVersion=47),
            dict(type='thread', processName='mach')]
    for name, start, end, data in (
        ('Phase', 1000, 40000, {'type': 'Phase', 'phase': 'compile'}),
        ('Phase', 2000, 20000, {'type': 'Phase', 'phase': 'export'}),
        ('SharedLib', 3000, 35000, {'type': 'Text', 'text': 'XUL'}),
        ('SharedLib', 1000, 10000, {'type': 'Text', 'text': 'XUL'}),
        ('Program', 4000, 30000, {'type': 'Text', 'text': 'xpcshell'}),
        ('Program', 2000, 15000, {'type': 'Text', 'text': 'xpcshell'})):
        if mode == 'no-end' and name == 'Phase':
            continue
        rows.append(dict(type='marker', name=name, startTime=start,
                         endTime=None if mode == 'no-end' else end, data=data))
    return rows


def fixture_serializer(module, path, profile_format, mode, *, large=False):
    rows = profile_rows(mode)
    if profile_format == 'jsonl':
        raw = ''.join(json.dumps(row) + '\n' for row in rows)
    else:
        markers = {key: [] for key in ('name', 'data', 'startTime', 'endTime',
                                       'phase', 'category', 'stack')}
        strings = []
        for row in rows[2:]:
            if row['name'] not in strings:
                strings.append(row['name'])
            markers['name'].append(strings.index(row['name']))
            for key in ('data', 'startTime', 'endTime'):
                markers[key].append(row[key])
            markers['phase'].append(0 if row['endTime'] is None else 1)
            markers['category'].append(0)
            markers['stack'].append(None)
        markers['length'] = len(rows) - 2
        profile = {'meta': rows[0], 'threads': [{'stringArray': strings, 'markers': markers}]}
        if large:
            profile['ignored'] = ['x' * contract.LARGE_PADDING_BYTES] * contract.LARGE_RESOURCE_ROWS
        raw = json.dumps(profile) + '\n'
    path.write_text(raw[:-2] if mode == 'truncated' else raw)
    return clean(), None


class SourceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()

    def test_validated_bytes_executed_only_after_exact_hash_check(self):
        source = self.root / 'source.py'
        raw = b'fixture_value = 17\n'
        source.write_bytes(raw)
        with mock.patch.object(contract, 'SOURCE_BYTES', len(raw)), \
                mock.patch.object(contract, 'SOURCE_SHA256', contract.hashlib.sha256(raw).hexdigest()):
            self.assertEqual(contract.load_serializer(source).fixture_value, 17)

    def test_wrong_hash_does_not_execute_untrusted_source(self):
        source = self.root / 'source.py'
        source.write_bytes(b'raise RuntimeError("PRIVATE-SOURCE")\n'.ljust(contract.SOURCE_BYTES, b' '))
        with self.assertRaisesRegex(contract.Failure, '^contract-source-hash$'):
            contract.load_serializer(source)

    def test_oversize_rejected_before_read_or_execute(self):
        source = self.root / 'source.py'
        with source.open('wb') as stream:
            stream.truncate(1024**2 + 1)
        with self.assertRaisesRegex(contract.Failure, '^contract-source-size$'):
            contract.load_serializer(source)

    def test_symlink_rejected(self):
        source = self.root / 'source.py'
        source.write_text('PRIVATE')
        link = self.root / 'link.py'
        link.symlink_to(source)
        with self.assertRaises(OSError):
            contract.load_serializer(link)

    def test_hardlink_rejected(self):
        source = self.root / 'source.py'
        source.write_text('PRIVATE')
        os.link(source, self.root / 'link.py')
        with self.assertRaisesRegex(contract.Failure, '^contract-source-kind$'):
            contract.load_serializer(source)

    def test_fifo_rejected_without_blocking_read(self):
        source = self.root / 'fifo'
        os.mkfifo(source)
        with self.assertRaisesRegex(contract.Failure, '^contract-source-kind$'):
            contract.load_serializer(source)


class CleanupTests(unittest.TestCase):
    def monitor(self):
        parent, child = multiprocessing.Pipe()
        process = multiprocessing.Process(target=None, args=(child,))
        monitor = types.SimpleNamespace(_stream_file=io.StringIO(), _pipe=parent, _process=process)
        return monitor, child

    def test_actual_unstarted_process_and_both_pipe_ends_closed_without_child_spawn(self):
        monitor, child = self.monitor()
        with mock.patch.object(multiprocessing.process.BaseProcess, 'start', side_effect=AssertionError('start')):
            result = contract.close_monitor(monitor)
        self.assertTrue(contract._clean(result))
        self.assertTrue(child.closed)
        self.assertTrue(monitor._pipe.closed)
        self.assertTrue(monitor._process._closed)

    def test_stream_close_failure_does_not_skip_independent_pipe_and_process_close(self):
        monitor, child = self.monitor()
        monitor._stream_file = mock.Mock()
        monitor._stream_file.close.side_effect = PermissionError(13, 'PRIVATE-PATH')
        result = contract.close_monitor(monitor)
        self.assertFalse(contract._clean(result))
        self.assertTrue(child.closed and monitor._pipe.closed and monitor._process._closed)
        self.assertEqual(result['failures'][0]['errno'], 13)
        self.assertNotIn('PRIVATE', json.dumps(result))

    def test_started_or_unknown_process_is_not_falsely_closed_or_qualified(self):
        process = mock.Mock(pid=12, _popen=object(), _args=())
        monitor = types.SimpleNamespace(_process=process, _pipe=None, _stream_file=io.StringIO())
        result = contract.close_monitor(monitor)
        self.assertFalse(result['workerNeverStarted'])
        self.assertFalse(result['processClosed'])
        self.assertFalse(contract._clean(result))
        process.close.assert_not_called()


class ObservationTests(unittest.TestCase):
    def test_projection_drops_private_extra_fields(self):
        value = {'status': 'unconfirmed', 'completeCoverage': False, 'PRIVATE': '/Users/secret',
                 'targets': {'XUL': {'status': 'unconfirmed', 'PRIVATE': 'argv-token'}},
                 'reason': 'PRIVATE-REASON'}
        projected = contract.observation_projection(value)
        self.assertNotIn('PRIVATE', json.dumps(projected))
        self.assertNotIn('secret', json.dumps(projected))
        self.assertFalse(projected['provesBuildCompletion'])

    def test_projection_preserves_unknown_and_rejects_boolean_numbers(self):
        value = {'status': 'observed', 'completeCoverage': 1,
                 'lastObservedFinishedTier': {'tier': 'PRIVATE', 'endTimeMs': True},
                 'targets': {'XUL': {'status': 'PRIVATE', 'endTimeMs': float('inf')}},
                 'finishedTargetMarkers': {'SharedLib': True, 'Program': -1}}
        projected = contract.observation_projection(value)
        self.assertIsNone(projected['completeCoverage'])
        self.assertIsNone(projected['lastFinishedTier']['endTimeMs'])
        self.assertEqual(projected['finishedTargetCounts'], {'SharedLib': None, 'Program': None})
        self.assertNotIn('PRIVATE', json.dumps(projected))

    def test_positive_requires_maximum_end_not_last_row(self):
        value = {'status': 'observed', 'profileFormat': 'jsonl', 'completeCoverage': True,
                 'lastFinishedTier': {'tier': 'export', 'endTimeMs': 20000},
                 'targets': {'XUL': {'status': 'observed-finished', 'endTimeMs': 35000},
                             'xpcshell': {'status': 'observed-finished', 'endTimeMs': 30000}},
                 'finishedTargetCounts': {'SharedLib': 2, 'Program': 2}}
        self.assertFalse(contract.satisfied(value, 'jsonl', 'positive'))
        value['lastFinishedTier'] = {'tier': 'compile', 'endTimeMs': 40000}
        self.assertTrue(contract.satisfied(value, 'jsonl', 'positive'))


class ContractTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()

    def run_fixture(self, task=None, serializer=fixture_serializer, *, large_whole=False):
        with mock.patch.object(contract, 'load_serializer', return_value=object()), \
                mock.patch.object(contract, 'serialize', side_effect=serializer):
            return contract.run_contract(self.root / 'fixture.py', task or self.root / 'probe',
                                         large_whole=large_whole)

    def test_known_fixtures_exercise_both_formats_and_negative_controls(self):
        value = self.run_fixture()
        self.assertEqual(value['status'], 'observed-both-formats')
        self.assertEqual(len(value['cases']), 6)
        self.assertTrue(all(row['expectationSatisfied'] for row in value['cases']))
        self.assertTrue(value['negativeInputsDidNotConfirmTargets'])
        self.assertFalse(value['realBuildPerformed'] or value['resourceQualification'])

    def test_optional_large_whole_contract_has_positive_noend_and_truncated_controls(self):
        value = self.run_fixture(large_whole=True)
        self.assertEqual(value['status'], 'observed-both-formats')
        self.assertTrue(value['largeWholeRequested'] and value['largeWholeObserved'])
        self.assertEqual(len(value['cases']), 9)
        large = [row for row in value['cases'] if row['largeProfile']]
        self.assertEqual({row['control'] for row in large}, set(contract.MODES))
        self.assertTrue(all(row['inputBytes'] > contract.PREVIOUS_WHOLE_LIMIT_BYTES and
                            row['expectationSatisfied'] and contract._clean(row['cleanup']) for row in large))
        self.assertTrue(all(row['format'] == 'whole-profile' for row in large))
        self.assertFalse(value['workerStarted'] or value['realBuildPerformed'] or value['mozillaQualification'])

    def test_large_contract_rejects_small_fixture_even_if_markers_are_valid(self):
        def undersized(*args, **kwargs):
            return fixture_serializer(*args)
        value = self.run_fixture(serializer=undersized, large_whole=True)
        self.assertEqual(value['status'], 'failed')
        self.assertEqual(value['firstFailure']['code'], 'contract-observation')
        self.assertFalse(value['largeWholeObserved'])
        self.assertEqual(len(value['cases']), 7)
        self.assertIsNone(value['cases'][-1]['observation'])

    def test_large_contract_requires_explicit_boolean_and_default_remains_six_cases(self):
        value = self.run_fixture()
        self.assertEqual(len(value['cases']), 6)
        self.assertFalse(value['largeWholeRequested'] or value['largeWholeObserved'])
        value = self.run_fixture(task=self.root / 'invalid', large_whole='yes')
        self.assertEqual(value['status'], 'failed')
        self.assertFalse(value['taskCreated'])

    def test_serializer_failure_retains_first_cause_and_independent_cleanup(self):
        error = PermissionError(13, 'PRIVATE-SERIALIZER')
        value = self.run_fixture(serializer=lambda *args: (clean(), error))
        self.assertEqual(value['status'], 'failed')
        self.assertEqual(value['firstFailure'], {'phase': 'serializer', 'code': 'contract-error',
                                                'kind': 'permission', 'errno': 13})
        self.assertTrue(contract._clean(value['cases'][0]['cleanup']))
        self.assertNotIn('PRIVATE', json.dumps(value))

    def test_cleanup_failure_prevents_collection_and_retains_failure(self):
        failed = clean() | {'parentPipeClosed': False}
        with mock.patch.object(contract, 'collect') as collect:
            value = self.run_fixture(serializer=lambda *args: (failed, None))
        collect.assert_not_called()
        self.assertEqual(value['firstFailure']['code'], 'contract-cleanup')
        self.assertFalse(value['cases'][0]['cleanup']['parentPipeClosed'])

    def test_existing_task_is_not_reused(self):
        task = self.root / 'probe'
        task.mkdir()
        sentinel = task / 'owned-by-other'
        sentinel.write_text('PRIVATE')
        value = self.run_fixture(task)
        self.assertFalse(value['taskCreated'])
        self.assertEqual(sentinel.read_text(), 'PRIVATE')
        self.assertEqual(list(task.iterdir()), [sentinel])

    def test_receipt_write_error_preserves_primary_and_emits_one_safe_marker(self):
        failed = {'status': 'failed', 'taskCreated': True, 'receiptWriteFailure': None,
                  'firstFailure': {'phase': 'serializer', 'code': 'contract-monitor-init'}}
        output = io.StringIO()
        with mock.patch.object(contract, 'run_contract', return_value=failed), \
                mock.patch.object(contract, 'write_json', side_effect=PermissionError(13, 'PRIVATE-WRITE')), \
                contextlib.redirect_stdout(output):
            code = contract.main(['--runtime-profile-source', 'unused', '--task', str(self.root / 'probe')])
        rows = output.getvalue().splitlines()
        self.assertEqual(code, 1)
        self.assertEqual(len(rows), 1)
        value = json.loads(rows[0])['buildCompletionContract']
        self.assertEqual(value['firstFailure']['code'], 'contract-monitor-init')
        self.assertEqual(value['receiptWriteFailure']['errno'], 13)
        self.assertNotIn('PRIVATE', rows[0])

    def test_failed_source_never_creates_or_writes_task(self):
        output = io.StringIO()
        task = self.root / 'probe'
        with contextlib.redirect_stdout(output), mock.patch.object(contract, 'write_json') as write:
            code = contract.main(['--runtime-profile-source', str(self.root / 'absent'), '--task', str(task)])
        self.assertEqual(code, 1)
        write.assert_not_called()
        self.assertFalse(task.exists())
        self.assertEqual(len(output.getvalue().splitlines()), 1)


if __name__ == '__main__':
    unittest.main()

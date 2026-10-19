# SPDX-License-Identifier: MPL-2.0
import contextlib
import ctypes
import errno
import io
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import task_api_experiment as experiment


PRIVATE_PID = 8675309
PRIVATE_UNIQUE = 98765432123456
ROW = (PRIVATE_PID, 123, 4, os.geteuid(), os.getuid())


def sensor(*, status=2, task_errno=0, unique_errno=0, exec_changes=False):
    library, system = mock.Mock(), mock.Mock()
    calls, unique_calls = [], 0

    def timebase(pointer):
        pointer._obj.numer = pointer._obj.denom = 1
        return 0
    system.mach_timebase_info.side_effect = timebase

    def query(pid, flavor, arg, pointer, size):
        nonlocal unique_calls
        calls.append((pid, flavor, arg, size))
        value = pointer._obj
        if flavor == 17:
            unique_calls += 1
            if unique_errno:
                ctypes.set_errno(unique_errno)
                return 0
            value.uuid[0] = 7
            value.uniqueid, value.parent_uniqueid = PRIVATE_UNIQUE, PRIVATE_UNIQUE + 1
            value.idversion = 2 if exec_changes and unique_calls > 1 else 1
            value.original_parent_version = -1
        elif flavor in (2, 3):
            if flavor == 2 and task_errno:
                ctypes.set_errno(task_errno)
                return 0
            bsd = value.bsd if flavor == 2 else value
            bsd.pid, bsd.start_sec, bsd.start_usec, bsd.uid, bsd.ruid = ROW
            bsd.status, bsd.flags = status, 4
            bsd.pgid, bsd.ppid = os.getpgrp(), os.getpid()
        if flavor in (2, 4):
            task = value.task if flavor == 2 else value
            task.virtual_size, task.resident_size = 1024**3, 1024**2
            task.policy, task.threadnum, task.numrunning = 1, 1, 0
        return size
    library.proc_pidinfo.side_effect = query
    log = experiment.TraceLog()
    log.phase, log.child = 'held-live', 0
    reader = experiment.TracedActivity(PRIVATE_PID, log, lib=library, system=system)
    # Private anchor returned out of band, never part of the receipt.
    anchor = (bytes([7] + [0] * 15), PRIVATE_UNIQUE, PRIVATE_UNIQUE + 1, 1, -1)
    return reader, library, calls, anchor


def empty_receipt():
    result = {}
    with mock.patch.object(experiment.sys, 'platform', 'linux'):
        try:
            experiment.run_experiment(Path('/tmp'), result)
        except experiment.ProbeFailure:
            pass
    result['firstFailure'] = None
    return result


class TaskAPITraceTests(unittest.TestCase):
    def test_real_query_wrapper_preserves_flavor_arguments_and_private_scope(self):
        reader, _, calls, _ = sensor()
        for flavor, (value_type, _) in experiment.FLAVORS.items():
            reader.query(PRIVATE_PID, flavor, value_type())
        self.assertEqual([(flavor, arg, size) for _, flavor, arg, size in calls],
                         [(2, 0, 232), (3, 1, 136), (4, 0, 96), (17, 0, 56)])
        rows = reader.trace.summary()['recent16']
        self.assertEqual([row['order'] for row in rows], [1, 2, 3, 4])
        self.assertTrue(all(row['returnBytes'] == row['bufferSize'] and row['errno'] == 0 for row in rows))
        self.assertTrue(all(row['startedSeconds'] <= row['finishedSeconds'] for row in rows))
        self.assertNotIn(str(PRIVATE_PID), json.dumps(rows))
        for pid, flavor, value in ((PRIVATE_PID + 1, 3, experiment.BSDInfo()),
                                   (True, 3, experiment.BSDInfo()),
                                   (PRIVATE_PID, 2, experiment.TaskInfo()),
                                   (PRIVATE_PID, 99, experiment.TaskInfo())):
            with self.assertRaises(experiment.ProbeFailure):
                reader.query(pid, flavor, value)
        reader.reaped = True
        with self.assertRaisesRegex(experiment.ProbeFailure, 'query-ownership'):
            reader.query(PRIVATE_PID, 3, experiment.BSDInfo())
        self.assertEqual(len(calls), 4)

    def test_production_internal_esrch_recheck_is_traced_without_task_four_fallback(self):
        reader, _, calls, anchor = sensor(task_errno=errno.ESRCH)
        value = experiment.snapshot(reader, ROW, anchor, 0)
        task_calls = [row for row in value['queryTrace'] if row['flavor'] == 2]
        self.assertEqual(len(task_calls), 2)
        self.assertTrue(all(row['returnBytes'] == 0 and row['errno'] == errno.ESRCH and row['arg'] == 0
                            for row in task_calls))
        self.assertTrue(value['fiveTupleBeforeAfterMatches'])
        self.assertTrue(value['uniqueExecMatchesSpawnAnchor'])
        self.assertTrue(value['sameGenerationLiveTaskAllESRCH'])
        self.assertTrue(value['execBoundSameGenerationLiveTaskAllESRCH'])
        self.assertEqual(value['production']['status'], 'unavailable')
        self.assertTrue(value['production']['esrchRecheck']['sameGeneration'])
        self.assertTrue(value['taskInfoComparison']['fullRecord'])
        self.assertFalse(value['taskInfoComparison']['measurementAuthority'])
        self.assertEqual(len(calls), 9)
        self.assertNotIn(str(PRIVATE_PID), json.dumps(value))
        self.assertNotIn(str(PRIVATE_UNIQUE), json.dumps(value))
        self.assertNotIn('_birth', json.dumps(value))

    def test_zombie_keeps_bsd_binding_and_unknown_exec_binding(self):
        reader, _, _, anchor = sensor(status=5, task_errno=errno.ESRCH, unique_errno=errno.ESRCH)
        reader.trace.phase = 'unreaped-zombie'
        value = experiment.snapshot(reader, ROW, anchor, 0)
        self.assertEqual(value['beforeBSD']['status'], 5)
        self.assertEqual(value['afterBSD']['status'], 5)
        self.assertTrue(value['fiveTupleBeforeAfterMatches'])
        self.assertIsNone(value['uniqueExecBeforeAfterMatches'])
        self.assertIsNone(value['uniqueExecMatchesSpawnAnchor'])
        self.assertEqual((value['production']['status'], value['production']['reason']), ('gone', 'bsd-zombie'))
        self.assertFalse(value['sameGenerationLiveTaskAllESRCH'])
        self.assertTrue(all(row['arg'] == (1 if row['flavor'] == 3 else 0) for row in value['queryTrace']))

    def test_same_birth_does_not_substitute_for_missing_or_changed_exec_identity(self):
        for settings, expected in (({'unique_errno': errno.ESRCH}, None), ({'exec_changes': True}, False)):
            reader, _, _, anchor = sensor(task_errno=errno.ESRCH, **settings)
            value = experiment.snapshot(reader, ROW, anchor, 0)
            self.assertTrue(value['sameGenerationLiveTaskAllESRCH'])
            self.assertIs(value['uniqueExecBeforeAfterMatches'], expected)
            self.assertIs(value['uniqueExecMatchesSpawnAnchor'], expected)
            self.assertFalse(value['execBoundSameGenerationLiveTaskAllESRCH'])

    def test_short_bsd_record_cannot_establish_generation_or_state(self):
        reader, library, _, anchor = sensor(task_errno=errno.ESRCH)
        original = library.proc_pidinfo.side_effect
        def short(pid, flavor, *args):
            if flavor == 3:
                return 135
            return original(pid, flavor, *args)
        library.proc_pidinfo.side_effect = short
        with mock.patch('darwin_activity.os.kill'):
            value = experiment.snapshot(reader, ROW, anchor, 0)
        self.assertIsNone(value['fiveTupleBeforeAfterMatches'])
        self.assertIsNone(value['beforeBSD']['status'])
        self.assertFalse(value['sameGenerationLiveTaskAllESRCH'])
        self.assertEqual(value['production']['status'], 'unavailable')

    def test_generation_change_is_failure_and_preserves_partial_sample(self):
        reader, library, _, anchor = sensor()
        original = library.proc_pidinfo.side_effect
        def replaced(pid, flavor, *args):
            size = original(pid, flavor, *args)
            if flavor == 3:
                args[1]._obj.start_sec += 1
            return size
        library.proc_pidinfo.side_effect = replaced
        result = empty_receipt()
        with self.assertRaisesRegex(experiment.ProbeFailure, 'generation-changed') as failure:
            experiment.snapshot(reader, ROW, anchor, 0)
        experiment.record_failure(result, reader.trace, failure.exception)
        self.assertFalse(result['stoppingSample']['beforeBSD']['sameSpawnFiveTuple'])
        self.assertFalse(result['stoppingSample']['completed'])
        self.assertEqual(result['firstFailure']['code'], 'task-api-generation-changed')

    def test_query_exception_preserves_exact_api_errno_phase_and_incomplete_sample(self):
        reader, library, _, anchor = sensor()
        original = library.proc_pidinfo.side_effect
        def broken(pid, flavor, *args):
            if flavor == 4:
                raise PermissionError(errno.EPERM, 'PRIVATE-TEXT-8675309')
            return original(pid, flavor, *args)
        library.proc_pidinfo.side_effect = broken
        result = empty_receipt()
        reader.trace.phase = 'exit-window'
        with self.assertRaises(PermissionError) as failure:
            experiment.snapshot(reader, ROW, anchor, 7)
        experiment.record_failure(result, reader.trace, failure.exception)
        value = reader.trace.summary()
        self.assertEqual(value['stoppingQuery']['flavor'], 4)
        self.assertEqual(value['stoppingQuery']['errno'], errno.EPERM)
        self.assertEqual(value['stoppingQuery']['phase'], 'exit-window')
        self.assertEqual(value['stoppingQuery']['sampleIndex'], 7)
        self.assertEqual(result['stoppingSample']['production']['status'], 'measured')
        self.assertIsNone(result['stoppingSample']['taskInfoComparison'])
        self.assertFalse(result['stoppingSample']['completed'])
        self.assertEqual(result['firstFailure']['code'], 'task-api-permission')
        self.assertNotIn('PRIVATE-TEXT', json.dumps([result, value]))

    def test_latest_sixteen_and_first_unavailable_survive_later_success(self):
        reader, library, _, anchor = sensor(task_errno=errno.ESRCH)
        result = empty_receipt()
        first = experiment.snapshot(reader, ROW, anchor, 0)
        experiment.retain_sample(result, first)
        original = library.proc_pidinfo.side_effect
        # Later complete calls are independent of the deliberately failed task2
        # setup. Retention is tested with fully formed immutable sample values.
        self.assertIsNotNone(original)
        for index in range(1, 22):
            value = {**first, 'sampleIndex': index,
                     'production': {**first['production'], 'status': 'measured', 'esrchRecheck': None},
                     'sameGenerationLiveTaskAllESRCH': False,
                     'execBoundSameGenerationLiveTaskAllESRCH': False}
            experiment.retain_sample(result, value)
            reader.query(PRIVATE_PID, 4, experiment.TaskInfo())
        self.assertEqual(result['sampleCount'], 22)
        self.assertEqual([row['sampleIndex'] for row in result['recent16Samples']], list(range(6, 22)))
        self.assertEqual(result['productionUnavailableSamples'], 1)
        self.assertIs(result['firstProductionUnavailable'], first)
        self.assertEqual(result['sameGenerationLiveTaskAllESRCHSamples'], 1)
        self.assertEqual(result['internalESRCHRecheckSamples'], 1)
        self.assertEqual(len(reader.trace.summary()['recent16']), 16)
        self.assertEqual(reader.trace.count, 30)
        self.assertEqual(reader.trace.counts['task-all-info']['errnoCounts'], {'3': 2})
        self.assertEqual(len(result['firstProductionUnavailable']['queryTrace']), 9)

    def test_query_sample_and_deadline_caps_fail_before_unbounded_collection(self):
        reader, library, _, _ = sensor()
        reader.trace.count = experiment.MAX_QUERIES
        with self.assertRaisesRegex(experiment.ProbeFailure, 'query-cap'):
            reader.query(PRIVATE_PID, 3, experiment.BSDInfo())
        library.proc_pidinfo.assert_not_called()
        reader.trace.count = 0
        reader.trace.clock = lambda: reader.trace.started + experiment.WORK_SECONDS
        with self.assertRaisesRegex(experiment.ProbeFailure, 'deadline'):
            reader.query(PRIVATE_PID, 3, experiment.BSDInfo())
        library.proc_pidinfo.assert_not_called()
        result = empty_receipt()
        result['sampleCount'] = experiment.MAX_SAMPLES
        with self.assertRaisesRegex(experiment.ProbeFailure, 'sample-cap'):
            experiment.retain_sample(result, {})
        self.assertEqual(result['sampleCount'], experiment.MAX_SAMPLES)

    def test_projection_has_fixed_allowlist_and_never_exports_raw_diagnostics(self):
        value = experiment.production_projection({'status': 'measured', '_birth': ROW[:3],
            'pidTag': 'PRIVATE-TEXT', 'identityTag': 'PRIVATE-TEXT', 'reason': 'PRIVATE-TEXT',
            'private': ROW, 'taskQuery': {'bytes': 232, 'expectedBytes': 232, 'errno': 0, 'pid': PRIVATE_PID},
            'esrchRecheck': {'sameGeneration': True, 'outcome': 'measured', 'identityTag': 'PRIVATE-TEXT',
                             'firstTaskQuery': {'bytes': 0, 'expectedBytes': 232, 'errno': 3}}}, ROW)
        self.assertTrue(value['sameSpawnBirth'])
        self.assertIsNone(value['reason'])
        self.assertTrue(value['esrchRecheck']['sameGeneration'])
        self.assertNotIn('PRIVATE-TEXT', json.dumps(value))
        self.assertNotIn(str(PRIVATE_PID), json.dumps(value))


class TaskAPIControlTests(unittest.TestCase):
    def test_actual_pipe_child_exits_naturally_and_exact_cleanup_reaps_on_linux_too(self):
        handles, child = {}, None
        cleanup = {}
        with tempfile.TemporaryDirectory() as temporary:
            try:
                child = experiment.spawn_child(Path(temporary), handles)
                experiment.close_fd(handles, 'controlRead')
                experiment.close_fd(handles, 'readyWrite')
                self.assertTrue(select.select([handles['readyRead']], [], [], 3)[0])
                self.assertEqual(os.read(handles['readyRead'], 1), b'R')
                self.assertEqual(os.getpgid(child.pid), os.getpgrp())
            finally:
                experiment.cleanup_child(child, handles, None, cleanup)
        self.assertTrue(cleanup['controlPipeClosed'])
        self.assertTrue(cleanup['leaderReaped'])
        self.assertIsNone(cleanup['groupGone'])
        self.assertTrue(cleanup['descriptorsClosed'])
        self.assertEqual(cleanup['groupCleanupScope'], 'outer-monitored-stage')
        self.assertEqual(cleanup['exitCode'], 0)
        self.assertEqual(cleanup['signalsSent'], [])
        self.assertEqual(cleanup['cleanupFailures'], [])
        self.assertTrue(all(value is None for value in handles.values()))

    def test_failure_cleanup_does_not_replace_binding_error_with_later_denial(self):
        result, log = empty_receipt(), experiment.TraceLog()
        child = mock.Mock(pid=PRIVATE_PID, returncode=None)
        def spawn(_task, handles):
            handles.update(controlRead=None, controlWrite=201, readyRead=202, readyWrite=None)
            return child
        def close(handles, name):
            handles[name] = None
        def cleanup(_child, _handles, _reader, receipt):
            receipt.update(leaderReaped=False, groupGone=None, descriptorsClosed=True,
                           cleanupFailures=[{'operation': 'wait-natural', 'kind': 'permission', 'errno': 1}])
        with mock.patch.object(experiment, 'spawn_child', side_effect=spawn), \
             mock.patch.object(experiment, 'close_fd', side_effect=close), \
             mock.patch.object(experiment.select, 'select', return_value=([202], [], [])), \
             mock.patch.object(experiment.os, 'read', return_value=b'R'), \
             mock.patch.object(experiment, 'TracedActivity', side_effect=OSError(errno.EIO, 'PRIVATE-TEXT')), \
             mock.patch.object(experiment, 'cleanup_child', side_effect=cleanup), self.assertRaises(OSError):
            experiment.exercise_child(Path('/tmp'), 0, log, result)
        self.assertEqual(result['firstFailure']['phase'], 'binding')
        self.assertEqual(result['firstFailure']['errno'], errno.EIO)
        self.assertEqual(result['children'][0]['cleanup']['cleanupFailures'][0]['kind'], 'permission')
        self.assertNotIn('PRIVATE-TEXT', json.dumps(result))

    def test_timeout_after_exact_kill_never_claims_reap_or_group_absence(self):
        child = mock.Mock(pid=PRIVATE_PID, returncode=None)
        child.wait.side_effect = subprocess.TimeoutExpired(['PRIVATE-TEXT'], 2)
        value = {}
        with mock.patch.object(experiment.os, 'killpg') as group:
            experiment.cleanup_child(child, {}, None, value)
        self.assertFalse(value['leaderReaped'])
        self.assertIsNone(value['groupGone'])
        self.assertIsNone(value['exitCode'])
        self.assertEqual(value['signalsSent'], ['SIGTERM', 'SIGKILL'])
        self.assertEqual(value['cleanupFailures'][0]['operation'], 'wait-after-kill')
        group.assert_not_called()
        self.assertNotIn('PRIVATE-TEXT', json.dumps(value))

    def test_exact_reap_does_not_claim_or_probe_the_inherited_stage_group_gone(self):
        child = mock.Mock(pid=PRIVATE_PID, returncode=0)
        value = {}
        with mock.patch.object(experiment.os, 'killpg') as group:
            experiment.cleanup_child(child, {}, None, value)
        self.assertTrue(value['leaderReaped'])
        self.assertTrue(value['descriptorsClosed'])
        self.assertIsNone(value['groupGone'])
        self.assertTrue(value['outerGroupCompletionRequired'])
        self.assertEqual(value['groupCleanupScope'], 'outer-monitored-stage')
        group.assert_not_called()
        child.terminate.assert_not_called()
        child.kill.assert_not_called()

    def test_spawn_failure_closes_both_pipes_without_inventing_a_child(self):
        handles, receipt = {}, {}
        with tempfile.TemporaryDirectory() as temporary, \
             mock.patch.object(experiment.subprocess, 'Popen', side_effect=OSError(errno.EIO, 'PRIVATE-TEXT')):
            with self.assertRaises(OSError):
                experiment.spawn_child(Path(temporary), handles)
            experiment.cleanup_child(None, handles, None, receipt)
        self.assertTrue(receipt['noChildCreated'])
        self.assertTrue(all(value is None for value in handles.values()))
        self.assertFalse(receipt['leaderReaped'])
        self.assertIsNone(receipt['groupGone'])

    def test_non_mac_is_a_recorded_failure_and_cli_writes_safe_result(self):
        with tempfile.TemporaryDirectory() as temporary, contextlib.redirect_stdout(io.StringIO()) as output:
            task = Path(temporary)
            with mock.patch.object(experiment.sys, 'platform', 'linux'), \
                 mock.patch.object(sys, 'argv', ['task_api_experiment.py', '--task', temporary]):
                self.assertEqual(experiment.main(), 1)
            result = json.loads((task / 'task-api-experiment.json').read_text())
        self.assertEqual(result['firstFailure']['code'], 'task-api-platform')
        self.assertFalse(result['actualDarwinExecution'])
        self.assertFalse(result['experimentCompleted'])
        self.assertFalse(result['repairEstablished'])
        self.assertEqual(result['run35FailureReproduction'], 'not-established')
        self.assertEqual(result['queryTrace']['totalQueries'], 0)
        self.assertEqual(json.loads(output.getvalue())['taskAPIExperiment'], result)

    def test_missing_evidence_write_keeps_primary_failure_and_nonzero_return(self):
        with tempfile.TemporaryDirectory() as temporary, contextlib.redirect_stdout(io.StringIO()) as output, \
             mock.patch.object(experiment.sys, 'platform', 'linux'), \
             mock.patch.object(experiment, 'write_json', side_effect=OSError('PRIVATE-TEXT')), \
             mock.patch.object(sys, 'argv', ['task_api_experiment.py', '--task', temporary]):
            self.assertEqual(experiment.main(), 1)
        result = json.loads(output.getvalue())['taskAPIExperiment']
        self.assertEqual(result['firstFailure']['code'], 'task-api-platform')
        self.assertTrue(result['evidenceWriteError'])
        self.assertNotIn('PRIVATE-TEXT', output.getvalue())

    def test_invalid_child_counts_do_not_spawn_or_claim_actual_mac(self):
        for count in (0, 33, True, 1.0):
            value = {}
            with mock.patch.object(experiment, 'exercise_child') as child, \
                 self.assertRaisesRegex(experiment.ProbeFailure, 'child-count'):
                experiment.run_experiment(Path('/tmp'), value, children=count)
            child.assert_not_called()
            self.assertFalse(value['actualDarwinExecution'])
            self.assertEqual(value['firstFailure']['phase'], 'platform')


if __name__ == '__main__':
    unittest.main()

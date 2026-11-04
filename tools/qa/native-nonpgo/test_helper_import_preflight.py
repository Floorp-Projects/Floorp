# SPDX-License-Identifier: MPL-2.0
import contextlib
import ast
import copy
import errno
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import helper_import_preflight as hp


def environment():
    return {key: 'PRIVATE-'+key for key in (*hp.BINDING_KEYS, 'TASK_ROOT', 'FLOORP_CENSUS_ABI')}


def reply(group):
    value = {'status': 'measured', 'group': group, 'pids': [987654321], 'queries': [],
        'measurement': {'api': 'sysctl-kern-proc-pgrp', 'snapshotBytes': 512,
            'snapshotEntries': 1, 'selfBindingVerified': True, 'layoutSHA256': 'a'*64}}
    return '{"inventoryProgress":"project-imports","sequence":1}\n'+json.dumps(value)+'\n'


def graph(variant):
    return json.dumps({'helperImportGraph': {'schema': 1, 'variant': variant,
        'moduleCount': 100, 'modules': {key: key != 'support' or variant == 'support-preimport'
                                     for key in hp.GRAPH_FIELDS}}})+'\n'


def measured(variant, phase, ordinal, *_args):
    return {'variant': variant, 'phase': phase, 'ordinal': ordinal, 'status': 'measured',
        'timeoutObserved': False, 'cleanup': {'reaped': True}, 'measurement': {'snapshotEntries': 1}}


def control_value():
    rows = []
    for index in range(2):
        rows.append({'index': index, 'status': 'passed', 'mode': 'fixed-iterations', 'iterations': 4,
            'sameGroupVerified': True, 'sufficient': True, 'fdClosed': True if index == 0 else None,
            'failurePresent': False, 'exitCode': 0, 'workerMiB': 1, 'diskMiB': 1 if index == 0 else 0,
            'touchedBytes': 1024**2, 'cpuIterations': 4, 'cpuHashedBytes': 4*1024**2,
            'diskInitialWriteBytes': 1024**2 if index == 0 else 0,
            'diskReadIterations': 4 if index == 0 else 0, 'diskReadBytes': 4*1024**2 if index == 0 else 0,
            'fsyncs': 5 if index == 0 else 0, 'elapsedSeconds': .01})
    return {'schema': 1, 'case': hp.WORKER_CONTROL_CASE, 'mode': 'fixed-iterations', 'iterations': 4,
        'status': 'passed', 'readyWorkers': 2, 'barriersReleased': 2, 'sameGroupVerified': True,
        'firstFailure': None, 'workerMeasurements': rows,
        'cleanup': [{'index': index, 'reaped': True, 'exitCode': 0, 'failure': None} for index in range(2)],
        'temporaryDiskRemoval': {'status': 'removed-after-exact-reap', 'failure': None},
        'temporaryDirectoryCleanup': {'status': 'removed-after-exact-reap', 'failure': None}}


def primary_anchor(code=0):
    return {'policy': 'retained-direct-child-v1', 'retirementVerified': True,
        'actualReaped': True, 'actualReturncode': code, 'signalAuthorityRevoked': True,
        'postReapAbsence': {'status': 'observed', 'groupEmpty': True,
                          'readOnly': True, 'signalAuthority': False},
        'scope': 'active-process-group-epoch', 'sessionOrDescendantClosure': 'unconfirmed'}


def stage_row():
    return {'stage': 'helper-import-workload', 'exitCode': 0, 'reason': None, 'failurePhase': None,
            'cleanupFailure': None, 'groupCleanup': 'verified-gone',
            'leaderReap': {'status': 'reaped', 'exitCode': 0}, 'primaryAnchor': primary_anchor()}


@contextlib.contextmanager
def timed_doubles(task):
    """Test timed orchestration without racing a subsecond real child lifetime."""
    clock = SimpleNamespace(now=0.)
    class Child:
        def __init__(self):
            self.returncode = None
            self.stdout, self.stdin = io.BytesIO(), io.BytesIO()
        def poll(self):
            if clock.now >= 60 and self.returncode is None:
                self.returncode = 0
            return self.returncode
        def wait(self, timeout=None):
            clock.now = max(clock.now, 60.)
            self.returncode = 0 if self.returncode is None else self.returncode
            return self.returncode
        def terminate(self): self.returncode = -15
        def kill(self): self.returncode = -9
    children = [Child(), Child()]
    next_child = iter(children)
    def spawn(args, **kwargs):
        if '--worker-iterations' in args or kwargs.get('start_new_session'):
            raise AssertionError('duration orchestration changed worker scope or mode')
        return next(next_child)
    def measurement(_task, index, seconds, mib, disk_mib):
        return {'index': index, 'status': 'passed', 'mode': 'duration', 'iterations': None,
            'seconds': seconds, 'workerMiB': mib, 'diskMiB': disk_mib,
            'touchedBytes': mib*1024**2, 'cpuIterations': 10, 'cpuHashedBytes': 10*mib*1024**2,
            'diskInitialWriteBytes': disk_mib*1024**2, 'diskReadIterations': 10 if disk_mib else 0,
            'diskReadBytes': 10*disk_mib*1024**2, 'fsyncs': 11 if disk_mib else 0,
            'elapsedSeconds': seconds, 'fdClosed': True if disk_mib else None,
            'failurePresent': False, 'sufficient': True}
    def advance(seconds): clock.now += seconds
    (task/'disk-control.bin').write_bytes(b'x'*1024**2)
    with mock.patch.object(hp.time, 'monotonic', side_effect=lambda: clock.now), \
         mock.patch.object(hp.time, 'sleep', side_effect=advance), \
         mock.patch.object(hp.subprocess, 'Popen', side_effect=spawn), mock.patch.object(hp, 'read_ready'), \
         mock.patch.object(hp, 'worker_measurement', side_effect=measurement):
        yield children


class SamplingTests(unittest.TestCase):
    def test_actual_fresh_wrapper_emits_parseable_stderr_graph_for_both_variants(self):
        for variant in hp.VARIANTS:
            with self.subTest(variant=variant):
                value = subprocess.run([sys.executable, '-S', '-c', hp.WRAPPER, variant,
                    str(hp.HERE/'darwin_inventory.py'), str(os.getpgrp())],
                    capture_output=True, text=True, check=True, timeout=3,
                    env=hp.helper_environment(environment()), cwd=hp.HERE)
                parsed = json.loads(value.stderr)
                self.assertIn('helperImportGraph', parsed)
                self.assertTrue(value.stderr.endswith('\n'))
                self.assertEqual(hp.import_graph(value.stderr, variant)['modules']['support'],
                                 variant == 'support-preimport')
                self.assertNotIn('PRIVATE', value.stderr)

    def test_environment_matches_runner_allowlist_and_excludes_tokens_path_pythonpath(self):
        original = {**environment(), 'HOME': '/PRIVATE', 'GITHUB_TOKEN': 'SECRET',
                    'SDK_READ_TOKEN': 'SECRET', 'ACTIONS_RUNTIME_TOKEN': 'SECRET',
                    'PYTHONPATH': '/PRIVATE', 'PATH': '/PRIVATE', 'DYLD_INSERT_LIBRARIES': '/PRIVATE'}
        value = hp.helper_environment(original)
        self.assertEqual(value['PYTHONDONTWRITEBYTECODE'], '1')
        for key in ('GITHUB_TOKEN', 'SDK_READ_TOKEN', 'ACTIONS_RUNTIME_TOKEN', 'PYTHONPATH', 'PATH', 'DYLD_INSERT_LIBRARIES'):
            self.assertNotIn(key, value)
        self.assertEqual(value['HOME'], '/PRIVATE')
        with self.assertRaises(hp.Failure): hp.helper_environment({})

    def test_fixed_wrapper_only_heavy_preimports_support_and_sample_has_strict_protocol(self):
        group = os.getpgrp()
        for variant in hp.VARIANTS:
            with self.subTest(variant=variant), mock.patch.object(hp.subprocess, 'run',
                return_value=SimpleNamespace(stdout=reply(group), stderr=graph(variant))) as run:
                value = hp.sample(variant, 'idle', 1, environment(), group)
            self.assertEqual(value['status'], 'measured'); self.assertTrue(value['cleanup']['reaped'])
            self.assertEqual(run.call_args.kwargs['timeout'], 3)
            self.assertEqual(run.call_args.args[0][1:3], ['-S', '-c'])
            self.assertTrue(run.call_args.kwargs['check'])
            self.assertEqual(run.call_args.args[0][3], hp.WRAPPER)
            self.assertNotIn('987654321', json.dumps(value)); self.assertNotIn('PRIVATE', json.dumps(value))
        self.assertIn("if variant=='support-preimport':\n import support", hp.WRAPPER)

    def test_timeout_is_unknown_not_zero_and_run_reaps_exact_helper(self):
        error = subprocess.TimeoutExpired(['PRIVATE-ARGV'], 3,
            output=b'{"inventoryProgress":"project-imports","sequence":1}\nPRIVATE')
        with mock.patch.object(hp.subprocess, 'run', side_effect=error):
            value = hp.sample('lean', 'contended', 2, environment(), os.getpgrp())
        self.assertEqual(value['status'], 'timeout'); self.assertTrue(value['timeoutObserved'])
        self.assertEqual(value['progress'], {'phase': 'project-imports', 'events': 1})
        self.assertIsNone(value['measurement']); self.assertIsNone(value['importGraph'])
        self.assertTrue(value['cleanup']['reaped']); self.assertTrue(value['cleanup']['killedOnTimeout'])
        self.assertNotIn('PRIVATE', json.dumps(value))

    def test_valid_unavailable_inventory_keeps_safe_query_cause_and_no_measurement(self):
        value = {'status': 'unavailable', 'group': os.getpgrp(), 'pids': None, 'queries': [],
            'failureCode': 'inventory-query-permission', 'phase': 'self-size', 'errorType': 'os-error',
            'errno': errno.EPERM}
        with mock.patch.object(hp.subprocess, 'run', return_value=SimpleNamespace(
                stdout=json.dumps(value)+'\n', stderr=graph('lean'))):
            result = hp.sample('lean', 'idle', 1, environment(), os.getpgrp())
        self.assertEqual(result['status'], 'inventory-unavailable')
        self.assertEqual(result['queryFailure']['failureCode'], 'inventory-query-permission')
        self.assertEqual(result['queryFailure']['errno'], errno.EPERM)
        self.assertIsNone(result['measurement']); self.assertTrue(result['cleanup']['reaped'])

    def test_timeout_then_permission_cleanup_failure_keeps_timeout_and_unknown_reap(self):
        def failing(*_args, **_kwargs):
            try:
                raise subprocess.TimeoutExpired(['PRIVATE'], 3,
                    output=b'{"inventoryProgress":"project-imports","sequence":1}\n')
            except subprocess.TimeoutExpired:
                raise PermissionError(errno.EPERM, 'PRIVATE-KILL')
        with mock.patch.object(hp.subprocess, 'run', side_effect=failing):
            value = hp.sample('support-preimport', 'idle', 1, environment(), os.getpgrp())
        self.assertEqual(value['status'], 'cleanup-unverified'); self.assertTrue(value['timeoutObserved'])
        self.assertIsNone(value['cleanup']['reaped'])
        self.assertEqual(value['cleanup']['failure']['errno'], errno.EPERM)
        self.assertEqual(value['progress']['phase'], 'project-imports')
        self.assertNotIn('PRIVATE', json.dumps(value))

    def test_spawn_failure_is_separate_from_protocol_nonzero_and_measurements_remain_unknown(self):
        for error, status in [(PermissionError(errno.EACCES, 'PRIVATE'), 'spawn-or-setup-failed'),
                (subprocess.CalledProcessError(2, ['PRIVATE'], output='PRIVATE'), 'nonzero-exit')]:
            with self.subTest(status=status), mock.patch.object(hp.subprocess, 'run', side_effect=error):
                value = hp.sample('lean', 'idle', 1, environment(), os.getpgrp())
            self.assertEqual(value['status'], status); self.assertIsNone(value['measurement'])
            self.assertNotIn('PRIVATE', json.dumps(value))
        with mock.patch.object(hp.subprocess, 'run', return_value=SimpleNamespace(stdout='PRIVATE', stderr='PRIVATE')):
            value = hp.sample('lean', 'idle', 1, environment(), os.getpgrp())
        self.assertEqual(value['status'], 'invalid-reply'); self.assertNotIn('PRIVATE', json.dumps(value))

    def test_protocol_does_not_accept_private_extra_keys_or_false_variant_graph(self):
        group = os.getpgrp(); value = json.loads(reply(group).splitlines()[-1]); value['PRIVATE'] = 'SECRET'
        with mock.patch.object(hp.subprocess, 'run', return_value=SimpleNamespace(stdout=json.dumps(value), stderr=graph('lean'))):
            row = hp.sample('lean', 'idle', 1, environment(), group)
        self.assertEqual(row['status'], 'invalid-reply'); self.assertIsNone(row['measurement'])
        wrong = json.loads(graph('lean')); wrong['helperImportGraph']['modules']['support'] = True
        with mock.patch.object(hp.subprocess, 'run', return_value=SimpleNamespace(stdout=reply(group), stderr=json.dumps(wrong))):
            row = hp.sample('lean', 'idle', 1, environment(), group)
        self.assertEqual(row['status'], 'invalid-reply')

    def test_argument_bounds_reject_nonfinite_oversized_or_foreign_group_without_spawn(self):
        for args in [(float('nan'), 2, 2, 1, 1), (91, 2, 2, 1, 1), (60, 1, 2, 1, 1),
                     (60, 2, 4, 1, 1), (60, 2, 3, 256, 1), (60, 2, 2, 1, 129)]:
            with self.subTest(args=args), self.assertRaises(hp.Failure): hp.validate_arguments(*args)
        with mock.patch.object(hp.subprocess, 'run') as run, self.assertRaises(hp.Failure):
            hp.sample('lean', 'idle', 1, environment(), os.getpgrp()+1234)
        run.assert_not_called()


class WorkloadTests(unittest.TestCase):
    def test_actual_small_workers_touch_memory_disk_and_exit_reaped_without_new_sessions(self):
        from inventory_stress import cleanup_children
        result = {'schema': 1, 'case': hp.WORKER_CONTROL_CASE, 'status': 'unavailable',
            'mode': 'fixed-iterations', 'iterations': 4, 'readyWorkers': 0, 'barriersReleased': 0,
            'sameGroupVerified': False, 'firstFailure': None, 'workerMeasurements': [], 'cleanup': [],
            'temporaryDiskRemoval': {'status': 'unverified', 'failure': None},
            'temporaryDirectoryCleanup': {'status': 'not-created', 'failure': None}}
        children, groups = [], []
        backing = primary = task = None
        all_launched_reaped = False
        phase = 'setup'
        def record(error):
            nonlocal primary
            if primary is None:
                primary = error
                result['firstFailure'] = {'phase': phase, 'code': 'helper-import-control-failed', **hp.safe_error(error)}
        try:
            task = Path(tempfile.mkdtemp(prefix='helper-import-control-'))
            result['temporaryDirectoryCleanup']['status'] = 'deferred-unverified'
            with mock.patch.dict(os.environ, environment()):
                phase = 'worker-launch'
                for index in range(2):
                    children.append(subprocess.Popen([sys.executable, str(Path(hp.__file__).resolve()), '--worker',
                        '--task', str(task), '--seconds', '60', '--mib', '1', '--worker-index', str(index),
                        '--worker-disk-mib', '1' if index == 0 else '0', '--worker-iterations', '4'],
                        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                        env=hp.helper_environment(os.environ)))
                phase = 'worker-ready'
                for child in children:
                    hp.read_ready(child)
                    result['readyWorkers'] += 1
                phase = 'worker-group'
                for child in children:
                    same = os.getpgid(child.pid) == os.getpgrp()
                    groups.append(same)
                    hp.require(same, 'helper-import-control-group')
                result['sameGroupVerified'] = all(groups) and len(groups) == 2
                backing = hp.disk_binding(task/'disk-control.bin', 1024**2)
                phase = 'worker-barrier'
                for child in children:
                    child.stdin.write(b'1'); child.stdin.flush(); child.stdin.close()
                    result['barriersReleased'] += 1
                phase = 'worker-wait'
                for child in children:
                    hp.require(child.wait(timeout=15) == 0, 'helper-import-worker-exit')
        except BaseException as error:
            record(error)
        finally:
            try:
                phase = 'cleanup'
                try:
                    rows = cleanup_children(children)
                    hp.require(isinstance(rows, list) and len(rows) == len(children) and
                        all(isinstance(row, dict) and type(row.get('index')) is int and row['index'] == index and
                            type(row.get('reaped')) is bool and
                            (row.get('exitCode') is None or type(row['exitCode']) is int) and
                            'failure' in row for index, row in enumerate(rows)), 'helper-import-child-cleanup')
                    result['cleanup'] = rows
                except BaseException as error:
                    record(error)
                    result['cleanup'] = [{'index': index, 'reaped': False, 'exitCode': None,
                        'failure': {'operation': 'collect-cleanup', **hp.safe_error(error)}}
                        for index in range(len(children))]
                all_launched_reaped = len(result['cleanup']) == len(children) and all(
                    row['reaped'] is True and type(row['exitCode']) is int and row['failure'] is None and
                    type(child.returncode) is int and child.returncode == row['exitCode'] and
                    not row.get('streamCloseFailed') and
                    all(stream is None or stream.closed is True for stream in (child.stdin, child.stdout))
                    for row, child in zip(result['cleanup'], children))
                if not all_launched_reaped:
                    record(hp.Failure('helper-import-child-cleanup'))
                phase = 'worker-measurements'
                for index, child in enumerate(children):
                    try:
                        value = hp.worker_measurement(task, index, 60, 1, 1 if index == 0 else 0, iterations=4)
                    except BaseException as error:
                        record(error)
                        value = {'index': index, 'status': 'unavailable', 'sufficient': False}
                    value['exitCode'] = result['cleanup'][index]['exitCode']
                    value['sameGroupVerified'] = index < len(groups) and groups[index] is True
                    result['workerMeasurements'].append(value)
                phase = 'disk-cleanup'
                cleanup_complete = len(children) == 2 and all_launched_reaped
                try:
                    if backing is not None and cleanup_complete:
                        hp.require(hp.disk_binding(task/'disk-control.bin', 1024**2) == backing, 'helper-import-disk-binding')
                        (task/'disk-control.bin').unlink()
                        hp.require(not os.path.lexists(task/'disk-control.bin'), 'helper-import-disk-removal')
                        result['temporaryDiskRemoval']['status'] = 'removed-after-exact-reap'
                    elif task is not None and not os.path.lexists(task/'disk-control.bin'):
                        result['temporaryDiskRemoval']['status'] = 'absent'
                except BaseException as error:
                    result['temporaryDiskRemoval']['failure'] = hp.safe_error(error)
                    record(error)
                phase = 'assertions'
                if primary is None:
                    self.assertEqual(result['readyWorkers'], 2); self.assertEqual(result['barriersReleased'], 2)
                    self.assertTrue(result['sameGroupVerified']); self.assertTrue(cleanup_complete)
                    self.assertTrue(all(row['exitCode'] == 0 for row in result['cleanup']))
                    self.assertEqual(result['temporaryDiskRemoval']['status'], 'removed-after-exact-reap')
                    self.assertEqual(len(result['workerMeasurements']), 2)
                    for index, row in enumerate(result['workerMeasurements']):
                        self.assertTrue(row['sufficient']); self.assertEqual(row['exitCode'], 0)
                        self.assertEqual(row['mode'], 'fixed-iterations'); self.assertEqual(row['iterations'], 4)
                        self.assertIsNone(row['seconds']); self.assertEqual(row['touchedBytes'], 1024**2)
                        self.assertEqual(row['cpuIterations'], 4); self.assertEqual(row['cpuHashedBytes'], 4*1024**2)
                        self.assertEqual(row['diskReadBytes'], 4*1024**2 if index == 0 else 0)
                        self.assertEqual(row['fsyncs'], 5 if index == 0 else 0)
                        self.assertIs(row['fdClosed'], True if index == 0 else None)
            except BaseException as error:
                record(error)
            finally:
                phase = 'temporary-cleanup'
                try:
                    if task is not None and all_launched_reaped:
                        shutil.rmtree(task)
                        hp.require(not os.path.lexists(task), 'helper-import-disk-removal')
                        result['temporaryDirectoryCleanup']['status'] = 'removed-after-exact-reap'
                except BaseException as error:
                    result['temporaryDirectoryCleanup'].update(status='failed', failure=hp.safe_error(error))
                    record(error)
                finally:
                    if primary is None:
                        try:
                            self.assertTrue(hp.worker_control_projection({**result, 'status': 'passed'})['valid'])
                        except BaseException as error:
                            record(error)
                    result['status'] = 'passed' if primary is None else 'failed'
                    print(json.dumps({'helperImportControlDiagnostic': hp.worker_control_projection(result)}), flush=True)
                if primary is not None:
                    raise primary

    def test_iterations_mode_is_never_a_duration_measurement_or_production_worker_argument(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()):
            task = Path(temporary)
            completed = subprocess.run([sys.executable, str(Path(hp.__file__).resolve()), '--worker',
                '--task', str(task), '--seconds', '60', '--mib', '1', '--worker-iterations', '4'],
                input=b'1', capture_output=True, timeout=3, env=hp.helper_environment(os.environ))
            self.assertEqual(completed.returncode, 0)
            finite = hp.worker_measurement(task, 0, 60, 1, 0, iterations=4)
            self.assertTrue(finite['sufficient']); self.assertEqual(finite['cpuIterations'], 4)
            self.assertIsNone(finite['seconds'])
            with self.assertRaises(hp.Failure) as denied:
                hp.worker_measurement(task, 0, 60, 1, 0)
            self.assertEqual(str(denied.exception), 'helper-import-worker-mode')
            for invalid in (True, 0, 3, 5):
                with self.subTest(iterations=invalid), self.assertRaises(hp.Failure):
                    hp.worker(task, 60, 1, iterations=invalid)
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()), \
             timed_doubles(Path(temporary)), mock.patch.object(hp, 'sample', side_effect=measured), \
             contextlib.redirect_stdout(io.StringIO()):
            timed = hp.workload(Path(temporary), seconds=60, pairs=2, workers=2, mib=1, disk_mib=1)
        self.assertEqual(timed['status'], 'passed'); self.assertEqual(timed['loadSeconds'], 60)
        self.assertEqual(timed['samplingReserveSeconds'], 8)
        self.assertEqual([row['variant'] for row in timed['samples']], list(hp.VARIANTS)*4)
        for row in timed['samples']:
            if row['phase'] == 'contended':
                self.assertTrue(row['loadBefore']['complete']); self.assertTrue(row['loadAfter']['complete'])

    def test_tiny_fixture_failure_emits_one_safe_marker_after_independent_reap_and_disk_cleanup(self):
        original = hp.worker_measurement
        def failure(task, index, *_args, **kwargs):
            if index == 0:
                raise PermissionError(errno.EACCES, 'PRIVATE-MEASUREMENT')
            return original(task, index, *_args, **kwargs)
        case = WorkloadTests('test_actual_small_workers_touch_memory_disk_and_exit_reaped_without_new_sessions')
        outcome = unittest.TestResult()
        with mock.patch.object(hp, 'worker_measurement', side_effect=failure), \
             contextlib.redirect_stdout(io.StringIO()) as output:
            case.run(outcome)
        self.assertEqual(len(outcome.errors), 1); self.assertEqual(len(outcome.failures), 0)
        self.assertEqual(len(output.getvalue().splitlines()), 1)
        receipt = json.loads(output.getvalue())['helperImportControlDiagnostic']
        self.assertEqual(receipt['status'], 'failed'); self.assertTrue(receipt['valid'])
        self.assertEqual(receipt['firstFailure']['phase'], 'worker-measurements')
        self.assertEqual(receipt['firstFailure']['kind'], 'permission')
        self.assertEqual(receipt['firstFailure']['errno'], errno.EACCES)
        self.assertTrue(all(row['reaped'] and row['exitCode'] == 0 for row in receipt['cleanup']))
        self.assertEqual(receipt['temporaryDiskRemoval']['status'], 'removed-after-exact-reap')
        self.assertNotIn('PRIVATE', output.getvalue())

    def test_tiny_fixture_failed_spawn_keeps_primary_and_still_emits_one_structured_marker(self):
        case = WorkloadTests('test_actual_small_workers_touch_memory_disk_and_exit_reaped_without_new_sessions')
        outcome = unittest.TestResult()
        with mock.patch.object(hp.subprocess, 'Popen', side_effect=PermissionError(errno.EACCES, 'PRIVATE-SPAWN')), \
             contextlib.redirect_stdout(io.StringIO()) as output:
            case.run(outcome)
        self.assertEqual(len(outcome.errors), 1); self.assertEqual(len(output.getvalue().splitlines()), 1)
        receipt = json.loads(output.getvalue())['helperImportControlDiagnostic']
        self.assertEqual(receipt['status'], 'failed'); self.assertTrue(receipt['valid'])
        self.assertEqual(receipt['firstFailure']['phase'], 'worker-launch')
        self.assertEqual(receipt['firstFailure']['errno'], errno.EACCES)
        self.assertEqual(receipt['cleanup'], []); self.assertEqual(receipt['workerMeasurements'], [])
        self.assertEqual(receipt['temporaryDiskRemoval']['status'], 'absent')
        self.assertNotIn('PRIVATE', output.getvalue())

    def test_tiny_fixture_short_cleanup_result_is_unknown_and_marker_still_reaches_outer_finally(self):
        from inventory_stress import cleanup_children
        for mode in ('short', 'unreaped', 'denied'):
            proven = []
            def incomplete(children):
                rows = cleanup_children(children)
                proven.extend(row['reaped'] and row['exitCode'] == 0 for row in rows)
                if mode == 'short':
                    return rows[:1]
                rows[0].update(reaped=False, exitCode=None, failure=None if mode == 'unreaped' else
                    {'operation': 'wait', 'kind': 'permission', 'errno': errno.EPERM})
                return rows
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as outer:
                task = Path(outer)/'control'; task.mkdir(mode=0o700)
                case = WorkloadTests('test_actual_small_workers_touch_memory_disk_and_exit_reaped_without_new_sessions')
                outcome = unittest.TestResult()
                with mock.patch.object(tempfile, 'mkdtemp', return_value=str(task)), \
                     mock.patch('inventory_stress.cleanup_children', side_effect=incomplete), \
                     mock.patch.object(shutil, 'rmtree') as remove, contextlib.redirect_stdout(io.StringIO()) as output:
                    case.run(outcome)
                self.assertEqual(len(outcome.errors), 1); self.assertEqual(len(output.getvalue().splitlines()), 1)
                receipt = json.loads(output.getvalue())['helperImportControlDiagnostic']
                self.assertTrue(receipt['valid']); self.assertEqual(receipt['status'], 'failed')
                self.assertEqual(receipt['firstFailure']['phase'], 'cleanup')
                self.assertEqual(receipt['firstFailure']['failureCode'], 'helper-import-child-cleanup')
                self.assertEqual(len(receipt['cleanup']), 2)
                self.assertFalse(receipt['cleanup'][0]['reaped']); self.assertIsNone(receipt['cleanup'][0]['exitCode'])
                self.assertEqual(receipt['temporaryDiskRemoval']['status'], 'unverified')
                self.assertEqual(receipt['temporaryDirectoryCleanup']['status'], 'deferred-unverified')
                remove.assert_not_called(); self.assertTrue(task.is_dir()); self.assertTrue((task/'disk-control.bin').is_file())
                # The injected receipt is unknown; independent real-child proof permits this test's outer teardown.
                self.assertEqual(proven, [True, True])
                if mode == 'denied':
                    self.assertEqual(receipt['cleanup'][0]['failure']['errno'], errno.EPERM)

    def test_tiny_fixture_directory_setup_failure_also_emits_exactly_one_safe_marker(self):
        case = WorkloadTests('test_actual_small_workers_touch_memory_disk_and_exit_reaped_without_new_sessions')
        outcome = unittest.TestResult()
        with mock.patch.object(tempfile, 'mkdtemp', side_effect=PermissionError(errno.EACCES, 'PRIVATE-SETUP')), \
             contextlib.redirect_stdout(io.StringIO()) as output:
            case.run(outcome)
        self.assertEqual(len(outcome.errors), 1); self.assertEqual(len(output.getvalue().splitlines()), 1)
        receipt = json.loads(output.getvalue())['helperImportControlDiagnostic']
        self.assertTrue(receipt['valid']); self.assertEqual(receipt['status'], 'failed')
        self.assertEqual(receipt['firstFailure']['phase'], 'setup')
        self.assertEqual(receipt['firstFailure']['errno'], errno.EACCES)
        self.assertEqual(receipt['cleanup'], []); self.assertNotIn('PRIVATE', output.getvalue())

    def test_tiny_fixture_directory_cleanup_failure_preserves_metrics_and_emits_single_failed_marker(self):
        directory = shutil.rmtree
        def fail(task):
            directory(task)
            raise PermissionError(errno.EACCES, 'PRIVATE-TEMPORARY-CLEANUP')
        case = WorkloadTests('test_actual_small_workers_touch_memory_disk_and_exit_reaped_without_new_sessions')
        outcome = unittest.TestResult()
        with mock.patch.object(shutil, 'rmtree', side_effect=fail), \
             contextlib.redirect_stdout(io.StringIO()) as output:
            case.run(outcome)
        self.assertEqual(len(outcome.errors), 1); self.assertEqual(len(output.getvalue().splitlines()), 1)
        receipt = json.loads(output.getvalue())['helperImportControlDiagnostic']
        self.assertTrue(receipt['valid']); self.assertEqual(receipt['status'], 'failed')
        self.assertEqual(receipt['firstFailure']['phase'], 'temporary-cleanup')
        self.assertEqual(receipt['firstFailure']['errno'], errno.EACCES)
        self.assertTrue(all(row['reaped'] and row['exitCode'] == 0 for row in receipt['cleanup']))
        self.assertTrue(all(row['sufficient'] for row in receipt['workerMeasurements']))
        self.assertEqual(receipt['temporaryDiskRemoval']['status'], 'removed-after-exact-reap')
        self.assertEqual(receipt['temporaryDirectoryCleanup']['status'], 'failed')
        self.assertEqual(receipt['temporaryDirectoryCleanup']['failure']['errno'], errno.EACCES)
        self.assertNotIn('PRIVATE', output.getvalue())

    def test_tiny_fixture_primary_survives_independent_directory_cleanup_denial(self):
        directory = shutil.rmtree
        def fail(task):
            directory(task)
            raise PermissionError(errno.EPERM, 'PRIVATE-SECONDARY')
        case = WorkloadTests('test_actual_small_workers_touch_memory_disk_and_exit_reaped_without_new_sessions')
        outcome = unittest.TestResult()
        with mock.patch.object(hp.subprocess, 'Popen', side_effect=PermissionError(errno.EACCES, 'PRIVATE-PRIMARY')), \
             mock.patch.object(shutil, 'rmtree', side_effect=fail), contextlib.redirect_stdout(io.StringIO()) as output:
            case.run(outcome)
        self.assertEqual(len(outcome.errors), 1); self.assertEqual(len(output.getvalue().splitlines()), 1)
        receipt = json.loads(output.getvalue())['helperImportControlDiagnostic']
        self.assertTrue(receipt['valid']); self.assertEqual(receipt['status'], 'failed')
        self.assertEqual(receipt['firstFailure']['phase'], 'worker-launch')
        self.assertEqual(receipt['firstFailure']['errno'], errno.EACCES)
        self.assertEqual(receipt['temporaryDirectoryCleanup']['failure']['errno'], errno.EPERM)
        self.assertNotIn('PRIVATE', output.getvalue())

    def test_partial_spawn_failure_still_reaps_first_owned_child_and_preserves_cause(self):
        child = mock.Mock(returncode=None); child.poll.return_value = None
        child.wait.return_value = 0
        def wait(*_args, **_kwargs): child.returncode = 0; return 0
        child.wait.side_effect = wait
        child.stdout = child.stdin = None
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()), \
             mock.patch.object(hp, 'sample', side_effect=measured), \
             mock.patch.object(hp.subprocess, 'Popen', side_effect=[child, PermissionError(errno.EACCES, 'PRIVATE')]), \
             contextlib.redirect_stdout(io.StringIO()):
            value = hp.workload(Path(temporary), seconds=.1, pairs=2, workers=2, mib=1, disk_mib=1)
        self.assertEqual(value['status'], 'failed'); self.assertEqual(value['firstFailure']['phase'], 'worker-launch')
        self.assertEqual(value['firstFailure']['errno'], errno.EACCES)
        child.terminate.assert_called_once(); self.assertTrue(value['cleanup'][0]['reaped'])
        self.assertNotIn('PRIVATE', json.dumps(value))

    def test_heavy_timeout_is_retained_but_lean_unknown_fails_and_cleanup_error_is_separate(self):
        def unknown(variant, phase, ordinal, *_args):
            value = measured(variant, phase, ordinal)
            if variant == 'support-preimport': value.update(status='timeout', timeoutObserved=True)
            return value
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()), \
             timed_doubles(Path(temporary)), mock.patch.object(hp, 'sample', side_effect=unknown), \
             contextlib.redirect_stdout(io.StringIO()):
            value = hp.workload(Path(temporary), seconds=60, pairs=2, workers=2, mib=1, disk_mib=1)
        self.assertEqual(value['status'], 'passed', value['firstFailure']); self.assertIsNone(value['firstFailure'])
        self.assertIsNotNone(value['firstControlFailure'])
        self.assertEqual(value['counts']['idle']['support-preimport']['timeouts'], 2)
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()), \
             mock.patch.object(hp, 'sample', side_effect=lambda v,p,n,*a:{**measured(v,p,n), 'status': 'timeout'}), \
             mock.patch.object(hp.subprocess, 'Popen', side_effect=PermissionError('PRIVATE')), \
             contextlib.redirect_stdout(io.StringIO()):
            failed = hp.workload(Path(temporary), seconds=.01, pairs=2, workers=2, mib=1, disk_mib=1)
        self.assertEqual(failed['status'], 'failed'); self.assertEqual(failed['firstFailure']['variant'], 'lean')

    def test_failed_worker_retains_partial_measurements_and_nonzero_exit(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()):
            task = Path(temporary); (task/'disk-control.bin').write_bytes(b'PRIVATE')
            completed = subprocess.run([sys.executable, str(Path(hp.__file__).resolve()), '--worker',
                '--task', str(task), '--seconds', '.1', '--mib', '1', '--worker-disk-mib', '1'],
                input=b'1', capture_output=True, timeout=3, env=hp.helper_environment(os.environ))
            row = hp.worker_measurement(task, 0, .1, 1, 1)
        self.assertEqual(completed.returncode, 1); self.assertFalse(row['sufficient'])
        self.assertEqual(row['status'], 'failed'); self.assertEqual(row['touchedBytes'], 1024**2)
        self.assertEqual(row['diskInitialWriteBytes'], 0); self.assertIsNone(row['elapsedSeconds'])
        self.assertTrue(row['failurePresent']); self.assertNotIn('PRIVATE', json.dumps(row))

    def test_insufficient_actual_work_is_failure_even_if_process_exits_zero(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()), \
             timed_doubles(Path(temporary)), mock.patch.object(hp, 'sample', side_effect=measured), \
             contextlib.redirect_stdout(io.StringIO()):
            original = hp.worker_measurement.side_effect
            def insufficient(*args):
                value = original(*args); value['sufficient'] = False; return value
            with mock.patch.object(hp, 'worker_measurement', side_effect=insufficient):
                value = hp.workload(Path(temporary), seconds=60, pairs=2, workers=2, mib=1, disk_mib=1)
        self.assertEqual(value['status'], 'failed'); self.assertEqual(value['firstFailure']['phase'], 'worker-measurements')
        self.assertTrue(all(row['exitCode'] == 0 for row in value['workerMeasurements']))
        self.assertEqual(value['temporaryDiskRemoval']['status'], 'removed-after-exact-reap')

    def test_before_and_after_load_observations_cannot_be_silently_discarded(self):
        original = hp.load_presence; calls = 0
        def vanished(children, expected):
            nonlocal calls
            calls += 1
            return original(children, expected) if calls == 1 else {
                'expectedWorkers': 2, 'runningWorkers': 1, 'complete': False, 'failure': None}
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()), \
             timed_doubles(Path(temporary)), mock.patch.object(hp, 'sample', side_effect=measured), \
             mock.patch.object(hp, 'load_presence', side_effect=vanished), contextlib.redirect_stdout(io.StringIO()):
            value = hp.workload(Path(temporary), seconds=60, pairs=2, workers=2, mib=1, disk_mib=1)
        self.assertEqual(value['status'], 'failed')
        self.assertTrue(value['loadChecks'][-1]['before']['complete'])
        self.assertFalse(value['loadChecks'][-1]['after']['complete'])
        self.assertTrue(all(row['reaped'] for row in value['cleanup']))

    def test_cleanup_failure_is_independent_and_never_deletes_unreaped_worker_disk(self):
        from inventory_stress import cleanup_children
        def cleanup(children):
            rows = cleanup_children(children)
            rows[0].update(reaped=False, failure={'operation': 'wait', 'kind': 'permission'})
            return rows
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()), \
             timed_doubles(Path(temporary)), mock.patch.object(hp, 'sample', side_effect=measured), \
             mock.patch('inventory_stress.cleanup_children', side_effect=cleanup), \
             contextlib.redirect_stdout(io.StringIO()):
            value = hp.workload(Path(temporary), seconds=60, pairs=2, workers=2, mib=1, disk_mib=1)
            self.assertTrue((Path(temporary)/'disk-control.bin').is_file())
        self.assertEqual(value['status'], 'failed'); self.assertEqual(value['firstFailure']['phase'], 'cleanup')
        self.assertEqual(value['temporaryDiskRemoval']['status'], 'unverified')

    def test_disk_binding_replacement_is_never_unlinked_as_original(self):
        with tempfile.TemporaryDirectory() as temporary:
            task = Path(temporary); path = task/'disk-control.bin'; path.write_bytes(b'x')
            identity = hp.disk_binding(path, 1)
            path.rename(task/'original.bin'); path.write_bytes(b'x')
            self.assertNotEqual(hp.disk_binding(path, 1), identity)
            path.unlink(); path.symlink_to(task/'original.bin')
            with self.assertRaises(hp.Failure): hp.disk_binding(path, 1)


class ControlProjectionTests(unittest.TestCase):
    def test_projection_is_pure_idempotent_bounded_and_drops_private_fields_at_every_level(self):
        value = control_value(); value.update(argv=['PRIVATE'], pid=987654321)
        value['workerMeasurements'][0].update(path='/PRIVATE', nonce='SECRET', rawPID=987654321)
        value['cleanup'][0].update(raw='PRIVATE', argv=['PRIVATE'])
        before = json.dumps(value, sort_keys=True)
        projected = hp.worker_control_projection(value)
        self.assertTrue(projected['valid']); self.assertEqual(projected['status'], 'passed')
        self.assertEqual(hp.worker_control_projection(projected), projected)
        self.assertEqual(json.dumps(value, sort_keys=True), before)
        self.assertNotIn('PRIVATE', json.dumps(projected)); self.assertNotIn('987654321', json.dumps(projected))
        self.assertNotIn('SECRET', json.dumps(projected))

    def test_failed_control_preserves_first_cause_and_cleanup_failure_independently(self):
        value = control_value(); value.update(status='failed', sameGroupVerified=False,
            firstFailure={'phase': 'worker-wait', 'code': 'helper-import-control-failed',
                'kind': 'timeout', 'errno': None, 'failureCode': None, 'message': 'PRIVATE'})
        value['cleanup'][0].update(reaped=False, exitCode=None,
            failure={'operation': 'wait-after-kill', 'kind': 'permission', 'errno': errno.EPERM, 'argv': 'PRIVATE'})
        value['temporaryDiskRemoval'] = {'status': 'unverified', 'failure': None, 'path': 'PRIVATE'}
        projected = hp.worker_control_projection(value)
        self.assertTrue(projected['valid']); self.assertEqual(projected['status'], 'failed')
        self.assertEqual(projected['firstFailure']['kind'], 'timeout')
        self.assertEqual(projected['cleanup'][0]['failure']['operation'], 'wait-after-kill')
        self.assertEqual(projected['cleanup'][0]['failure']['errno'], errno.EPERM)
        self.assertIsNone(projected['cleanup'][0]['exitCode'])
        self.assertEqual(hp.worker_control_projection(projected), projected)
        self.assertNotIn('PRIVATE', json.dumps(projected))

    def test_malformed_or_inconsistent_controls_stay_invalid_on_reprojection_without_secret_leaks(self):
        changes = [{'schema': True}, {'case': 'PRIVATE'}, {'mode': 'duration'}, {'iterations': True},
            {'readyWorkers': True}, {'barriersReleased': -1}, {'sameGroupVerified': 987654321},
            {'firstFailure': {'phase': 'PRIVATE', 'code': 'PRIVATE', 'kind': 'PRIVATE', 'errno': 'PRIVATE'}},
            {'workerMeasurements': [control_value()['workerMeasurements'][0]]*3},
            {'cleanup': [{'index': 0, 'reaped': False, 'exitCode': None, 'failure': None}]},
            {'temporaryDiskRemoval': {'status': 'PRIVATE'}}, {'valid': False}]
        for change in changes:
            with self.subTest(change=change):
                value = hp.worker_control_projection({**control_value(), **change})
                self.assertFalse(value['valid']); self.assertEqual(hp.worker_control_projection(value), value)
                self.assertNotIn('PRIVATE', json.dumps(value)); self.assertNotIn('987654321', json.dumps(value))
        for counter in (True, float('nan'), float('inf'), -1, 2**10000):
            value = control_value(); value['workerMeasurements'][0]['cpuIterations'] = counter
            projected = hp.worker_control_projection(value)
            self.assertFalse(projected['valid']); self.assertEqual(hp.worker_control_projection(projected), projected)
        for raw in (None, [], 'PRIVATE'):
            projected = hp.worker_control_projection(raw)
            self.assertFalse(projected['valid']); self.assertEqual(hp.worker_control_projection(projected), projected)

    def test_safe_failure_code_is_allowlisted_and_unknown_exception_text_is_never_copied(self):
        known = hp.safe_error(hp.Failure('helper-import-load-ended-early'))
        unknown = hp.safe_error(hp.Failure('PRIVATE-PATH-ARGV-NONCE'))
        self.assertEqual(known['failureCode'], 'helper-import-load-ended-early')
        self.assertEqual(known['kind'], 'failure'); self.assertIsNone(unknown['failureCode'])
        self.assertNotIn('PRIVATE', json.dumps(unknown))


class DriverTests(unittest.TestCase):
    def test_fixed_primary_failure_codes_and_phase_survive_without_arbitrary_text(self):
        for code in hp.PRIMARY_FAILURE_CODES:
            with self.subTest(code=code):
                failure = hp.runner_failure({'stage': 'helper-import-workload', 'code': code,
                    'elapsedSeconds': 1.5, 'pid': 987654321, 'path': '/PRIVATE-SENTINEL'})
                stage = hp.stage_projection({'stage': 'helper-import-workload', 'reason': code,
                    'failurePhase': 'primary-observation', 'cleanupFailure': code})
                self.assertEqual(failure['code'], code)
                self.assertEqual(stage['reason'], code)
                self.assertEqual(stage['failurePhase'], 'primary-observation')
                self.assertEqual(stage['cleanupFailureCode'], code)
                self.assertNotIn('PRIVATE-SENTINEL', json.dumps([failure, stage]))
                self.assertNotIn('987654321', json.dumps([failure, stage]))
        for unknown in ('owned-primary-PRIVATE-SENTINEL', True, ['PRIVATE-SENTINEL']):
            self.assertEqual(hp.runner_failure({'code': unknown})['code'], 'runner-failure')
            value = hp.stage_projection({'reason': unknown, 'failurePhase': unknown,
                                        'cleanupFailure': unknown})
            self.assertEqual(value['reason'], 'runner-failure')
            self.assertEqual(value['failurePhase'], 'unavailable')
            self.assertNotIn('PRIVATE-SENTINEL', json.dumps(value))

    def test_anchor_cleanup_history_and_separate_stopping_detail_share_safe_projection(self):
        stopping = {'operation': 'anchor-observation', 'status': 'unavailable', 'state': 'lost',
            'reason': 'anchor-echild', 'actualReaped': None, 'actualReturncode': None,
            'observedExitCode': None, 'signalAuthority': False, 'freshIdentityVerified': False,
            'errorType': 'os-error', 'errno': errno.ECHILD, 'pid': 987654321,
            'argv': ['/PRIVATE-SENTINEL'], 'private': 'PRIVATE-SENTINEL'}
        later = {'operation': 'anchor-exact-recovery', 'status': 'lost', 'state': 'lost',
            'reason': 'anchor-recovery-unbound', 'actualReaped': None,
            'signalAuthority': False, 'errorType': 'anchor-error', 'errno': None,
            'group': 987654321, 'path': '/PRIVATE-SENTINEL'}
        value = hp.stage_projection({'cleanupFailure': 'owned-primary-anchor-unavailable',
            'cleanupDiagnostics': [stopping] + [later] * 20,
            'cleanupStoppingDiagnostic': stopping})
        self.assertEqual(len(value['cleanupDiagnostics']), 16)
        self.assertTrue(all(row['operation'] == 'anchor-exact-recovery'
                            for row in value['cleanupDiagnostics']))
        self.assertEqual(value['cleanupStoppingDiagnostic']['reason'], 'anchor-echild')
        self.assertEqual(value['cleanupStoppingDiagnostic']['errno'], errno.ECHILD)
        self.assertIsNone(value['cleanupStoppingDiagnostic']['actualReaped'])
        self.assertEqual(value['cleanupFailureCode'], 'owned-primary-anchor-unavailable')
        self.assertEqual(hp.cleanup_diagnostic_projection(value['cleanupStoppingDiagnostic']),
                         value['cleanupStoppingDiagnostic'])
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(value))
        self.assertNotIn('987654321', json.dumps(value))

    def test_anchor_diagnostics_reject_unknown_types_and_never_supply_primary_proof(self):
        for operation in ('anchor-stop-refused', 'anchor-group-signal', 'anchor-retirement',
                          'anchor-post-reap-absence', 'anchor-exact-recovery'):
            value = hp.cleanup_diagnostic_projection({'operation': operation,
                'reason': 'anchor-authority-revoked', 'errorType': 'interrupted'})
            self.assertEqual(value['operation'], operation)
            self.assertEqual(value['reason'], 'anchor-authority-revoked')
        raw = {'operation': ['PRIVATE-SENTINEL'], 'reason': '/PRIVATE-SENTINEL',
            'errorType': True, 'status': False, 'state': ['PRIVATE-SENTINEL'],
            'errno': True, 'actualReturncode': False, 'observedExitCode': True,
            'signal': True, 'actualReaped': 1, 'signalAuthority': 1,
            'freshIdentityVerified': 0, 'groupEmpty': 1, 'readOnly': 1}
        value = hp.cleanup_diagnostic_projection(raw)
        for key in ('operation', 'reason', 'errorType', 'status', 'state'):
            self.assertEqual(value[key], 'unavailable')
        for key in ('errno', 'actualReturncode', 'observedExitCode', 'signal', 'actualReaped',
                    'signalAuthority', 'freshIdentityVerified', 'groupEmpty', 'readOnly'):
            self.assertIsNone(value[key])
        self.assertEqual(hp.cleanup_diagnostic_projection(value), value)
        self.assertIsNone(hp.cleanup_diagnostic_projection(False))
        row = {'exitCode': 0, 'groupCleanup': 'verified-gone',
            'leaderReap': {'status': 'reaped', 'exitCode': 0}, 'reason': None,
            'failurePhase': None, 'cleanupFailure': None, 'cleanupStoppingDiagnostic': value}
        self.assertFalse(hp.stage_complete(row))
        self.assertFalse(hp.stage_complete(hp.stage_projection(row)))
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(value))

    def test_inventory_timeout_cause_and_progress_survive_later_anchor_cleanup_without_private_fields(self):
        stopping = {'operation': 'anchor-group-inventory', 'backend': 'darwin-header-bound',
            'failureCode': 'anchor-inventory-timeout', 'errorType': 'TimeoutExpired',
            'phase': 'abi-binding', 'timeoutSeconds': 3,
            'helperProgress': {'phase': 'project-imports', 'events': 1,
                               'path': '/PRIVATE-SENTINEL', 'pid': 987654321},
            'argv': ['/PRIVATE-SENTINEL'], 'raw': 'PRIVATE-SENTINEL'}
        later = [{'operation': 'anchor-retirement', 'reason': 'anchor-nonleaders-remain'},
                 {'operation': 'anchor-stop-refused', 'reason': 'anchor-echild'},
                 {'operation': 'anchor-exact-recovery', 'reason': 'anchor-recovery-unbound'}] * 7
        value = hp.stage_projection({'reason': 'owned-primary-reap-unconfirmed',
            'cleanupFailure': 'owned-primary-anchor-unavailable',
            'cleanupDiagnostics': [stopping] + later, 'cleanupStoppingDiagnostic': stopping})
        self.assertEqual(value['reason'], 'owned-primary-reap-unconfirmed')
        self.assertEqual(len(value['cleanupDiagnostics']), 16)
        self.assertFalse(any(item['operation'] == 'anchor-group-inventory'
                             for item in value['cleanupDiagnostics']))
        stop = value['cleanupStoppingDiagnostic']
        self.assertEqual(stop['backend'], 'darwin-header-bound')
        self.assertEqual(stop['failureCode'], 'anchor-inventory-timeout')
        self.assertEqual(stop['phase'], 'abi-binding')
        self.assertEqual(stop['timeoutSeconds'], 3)
        self.assertEqual(stop['helperProgress'], {'phase': 'project-imports', 'events': 1})
        self.assertEqual(hp.cleanup_diagnostic_projection(stop), stop)
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(value))
        self.assertNotIn('987654321', json.dumps(value))
        for unknown in ('/PRIVATE-SENTINEL', True, ['PRIVATE-SENTINEL']):
            bad = hp.cleanup_diagnostic_projection({'operation': 'anchor-group-inventory',
                'backend': unknown, 'failureCode': unknown, 'phase': unknown,
                'timeoutSeconds': unknown, 'helperProgress': {'phase': unknown, 'events': unknown}})
            self.assertEqual(bad['backend'], 'unavailable')
            self.assertEqual(bad['failureCode'], 'unavailable')
            self.assertEqual(bad['phase'], 'unobserved')
            self.assertIsNone(bad['timeoutSeconds'])
            self.assertEqual(bad['helperProgress'], {'phase': 'unobserved', 'events': None})
            self.assertNotIn('PRIVATE-SENTINEL', json.dumps(bad))
        for bad_count in (-1, hp.MAX_PROGRESS_EVENTS + 1, True):
            projected = hp.cleanup_diagnostic_projection({'helperProgress': {
                'phase': 'project-imports', 'events': bad_count}})
            self.assertIsNone(projected['helperProgress']['events'])

    def test_native_producer_failure_stays_separate_from_adapter_and_unknown_text(self):
        source = ast.parse(Path(hp.__file__).with_name('darwin_inventory.py').read_text())
        decoder = next(node for node in source.body
                       if isinstance(node, ast.FunctionDef) and node.name == 'decode_reply')
        vocabulary = next(node for node in ast.walk(decoder) if isinstance(node, ast.Set) and
            all(isinstance(item, ast.Constant) and type(item.value) is str and
                item.value.startswith('inventory-') for item in node.elts))
        self.assertEqual(set(hp.NATIVE_INVENTORY_FAILURE_CODES),
                         {node.value for node in vocabulary.elts})
        for code in hp.NATIVE_INVENTORY_FAILURE_CODES:
            stopping = {'operation': 'anchor-group-inventory', 'backend': 'darwin-header-bound',
                'failureCode': 'anchor-inventory-unavailable', 'producerFailureCode': code,
                'phase': 'abi-binding', 'errorType': 'failure', 'errno': errno.EACCES,
                'pid': 987654321, 'path': '/PRIVATE-SENTINEL'}
            projected = hp.stage_projection({'cleanupDiagnostics': [stopping] +
                [{'operation': 'anchor-stop-refused', 'reason': 'anchor-echild'}] * 20,
                'cleanupStoppingDiagnostic': stopping})
            stop = projected['cleanupStoppingDiagnostic']
            self.assertEqual(stop['failureCode'], 'anchor-inventory-unavailable')
            self.assertEqual(stop['producerFailureCode'], code)
            self.assertEqual(stop['phase'], 'abi-binding')
            self.assertEqual(stop['errno'], errno.EACCES)
            self.assertEqual(hp.cleanup_diagnostic_projection(stop), stop)
            self.assertNotIn('PRIVATE-SENTINEL', json.dumps(projected))
            self.assertNotIn('987654321', json.dumps(projected))
        for unknown in ('/PRIVATE-SENTINEL', 'anchor-inventory-timeout', True, ['PRIVATE-SENTINEL']):
            value = hp.cleanup_diagnostic_projection({'producerFailureCode': unknown})
            self.assertEqual(value['producerFailureCode'], 'unavailable')
            self.assertNotIn('PRIVATE-SENTINEL', json.dumps(value))

    def test_inventory_failure_slot_is_independent_of_stop_and_unknown_is_not_zero(self):
        cause = {'operation': 'anchor-group-inventory', 'backend': 'darwin-header-bound',
            'failureCode': 'anchor-inventory-unavailable',
            'producerFailureCode': 'inventory-abi-unavailable', 'phase': 'abi-binding',
            'errorType': 'failure', 'errno': True, 'pid': 987654321,
            'helperProgress': {'phase': 'abi-binding', 'events': True,
                               'argv': ['/PRIVATE-SENTINEL']}}
        stop = {'operation': 'anchor-retirement', 'reason': 'anchor-census-unavailable'}
        row = hp.stage_projection({'cleanupStoppingDiagnostic': stop,
            'cleanupInventoryFailure': cause, 'cleanupDiagnostics': [stop] * 20})
        self.assertEqual(row['cleanupStoppingDiagnostic'], stop)
        self.assertEqual(row['cleanupInventoryFailure']['producerFailureCode'], 'inventory-abi-unavailable')
        self.assertIsNone(row['cleanupInventoryFailure']['errno'])
        self.assertIsNone(row['cleanupInventoryFailure']['helperProgress']['events'])
        self.assertEqual(len(row['cleanupDiagnostics']), 16)
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(row))
        self.assertNotIn('987654321', json.dumps(row))
        for unavailable in (None, False, ['/PRIVATE-SENTINEL']):
            self.assertIsNone(hp.stage_projection({'cleanupInventoryFailure': unavailable})[
                'cleanupInventoryFailure'])

    def test_stage_gate_rejects_old_missing_unknown_and_forged_anchor_proofs(self):
        value = stage_row()
        self.assertTrue(hp.stage_complete(value))
        variants = []
        old = copy.deepcopy(value); del old['primaryAnchor']; variants.append(old)
        for key in hp.PRIMARY_ANCHOR_KEYS:
            item = copy.deepcopy(value); del item['primaryAnchor'][key]; variants.append(item)
        for key in hp.PRIMARY_ABSENCE_KEYS:
            item = copy.deepcopy(value); del item['primaryAnchor']['postReapAbsence'][key]; variants.append(item)
        for key, bad in (('policy', 'PRIVATE'), ('retirementVerified', 1), ('actualReaped', None),
                         ('actualReaped', 1), ('actualReturncode', False), ('actualReturncode', 1),
                         ('signalAuthorityRevoked', False), ('signalAuthorityRevoked', 1),
                         ('scope', 'all-descendants'), ('sessionOrDescendantClosure', 'verified'),
                         ('argv', ['PRIVATE'])):
            item = copy.deepcopy(value); item['primaryAnchor'][key] = bad; variants.append(item)
        for key, bad in (('status', 'unconfirmed'), ('groupEmpty', None), ('groupEmpty', 1),
                         ('readOnly', False), ('readOnly', 1), ('signalAuthority', True),
                         ('signalAuthority', 0), ('path', '/PRIVATE')):
            item = copy.deepcopy(value); item['primaryAnchor']['postReapAbsence'][key] = bad
            variants.append(item)
        for item in variants:
            with self.subTest(proof=item.get('primaryAnchor')):
                self.assertFalse(hp.stage_complete(item))

    def test_anchor_projection_is_typed_private_and_cannot_launder_extra_fields(self):
        source = stage_row()
        self.assertEqual(hp.stage_projection(source)['primaryAnchor'], primary_anchor())
        self.assertEqual(hp.primary_anchor_projection(hp.primary_anchor_projection(primary_anchor())), primary_anchor())
        for where in ('outer', 'absence'):
            item = copy.deepcopy(source)
            target = item['primaryAnchor'] if where == 'outer' else item['primaryAnchor']['postReapAbsence']
            target['PRIVATE'] = '/PRIVATE/argv/987654321'
            projected = hp.stage_projection(item)
            self.assertIsNone(projected['primaryAnchor'])
            self.assertNotIn('PRIVATE', json.dumps(projected))
            self.assertNotIn('987654321', json.dumps(projected))
            self.assertFalse(hp.primary_anchor_complete(projected['primaryAnchor'], 0))
        unknown = primary_anchor()
        unknown.update(policy='PRIVATE', actualReaped=None, actualReturncode=False, scope='PRIVATE')
        unknown['postReapAbsence'].update(groupEmpty=None, signalAuthority=0)
        projected = hp.primary_anchor_projection(unknown)
        self.assertIsNone(projected['policy']); self.assertIsNone(projected['scope'])
        self.assertIsNone(projected['actualReaped']); self.assertIsNone(projected['actualReturncode'])
        self.assertIsNone(projected['postReapAbsence']['groupEmpty'])
        self.assertIsNone(projected['postReapAbsence']['signalAuthority'])
        self.assertEqual(hp.primary_anchor_projection(projected), projected)
        self.assertFalse(hp.primary_anchor_complete(projected, 0))

    def test_driver_cannot_promote_old_or_forged_stage_shape_to_success(self):
        import support
        variants = [[], [stage_row()], [stage_row()]]
        del variants[1][0]['primaryAnchor']
        variants[2][0]['primaryAnchor']['postReapAbsence']['groupEmpty'] = None
        for records in variants:
            def run(_stage, args, *_args, **_kwargs):
                hp.write_json(Path(args[-1])/'workload.json', {'status': 'passed'})
            runner = SimpleNamespace(failure=None, monitor_failure=None, records=records, run=run,
                                     monitor_diagnostics=[])
            with self.subTest(records=records), tempfile.TemporaryDirectory() as temporary, \
                 mock.patch.object(hp.sys, 'platform', 'darwin'), mock.patch.object(support, 'Runner', return_value=runner), \
                 mock.patch.object(support, 'clean_environment', return_value={}), mock.patch.object(hp, 'select_abi', return_value=True), \
                 contextlib.redirect_stdout(io.StringIO()):
                task = Path(temporary); (task/'started').write_text('1')
                value = hp.run_experiment(task, {})
            self.assertEqual(value['status'], 'failed')
            self.assertIsNotNone(value['firstFailure'])

    def test_monitor_projection_keeps_stop_query_partial_progress_and_unknown_values_without_secrets(self):
        partial = {'unknownKinds': {'inventory-timeout': 1, 'PRIVATE': 7},
            'formatExamples': ['PRIVATE'], 'knownRSSLowerBoundMiB': None,
            'queryFailure': {'operation': 'kern-proc-pgrp-inventory', 'errorType': 'TimeoutExpired',
                'timeoutSeconds': 3, 'elapsedSeconds': 3.007, 'argv': ['PRIVATE'],
                'helperProgress': {'phase': 'project-imports', 'events': 1, 'path': 'PRIVATE'}}}
        value = hp.monitor_projection({'stage': 'helper-import-workload', 'sampleComplete': False,
            'sampleAttempts': 3, 'rssMiB': None, 'cpuSeconds': None, '_pids': [987654321],
            'unknownKinds': {'inventory-timeout': 1}, 'partialSamples': [partial]*3,
            'queryFailure': partial['queryFailure'], 'PRIVATE': 'SECRET'})
        self.assertEqual(len(value['partialSamples']), 3); self.assertIsNone(value['rssMiB'])
        self.assertEqual(value['queryFailure']['helperProgress'], {'phase': 'project-imports', 'events': 1})
        self.assertEqual(value['partialSamples'][0]['queryFailure']['elapsedSeconds'], 3.007)
        self.assertEqual(value['partialSamples'][0]['unknownKinds'], {'inventory-timeout': 1})
        self.assertNotIn('PRIVATE', json.dumps(value)); self.assertNotIn('987654321', json.dumps(value))

    def test_runner_first_failure_wins_generic_followup_and_monitor_receipt_is_not_reduced_to_flag(self):
        import support
        runner = SimpleNamespace(failure={'stage': 'helper-import-workload', 'code': 'monitor-unavailable',
            'elapsedSeconds': 3.5, 'PRIVATE': 'SECRET'}, records=[], monitor_diagnostics=[],
            monitor_failure={'stage': 'helper-import-workload', 'sampleComplete': False,
                'sampleAttempts': 3, 'partialSamples': [], 'queryFailure': {
                    'operation': 'kern-proc-pgrp-inventory', 'errorType': 'TimeoutExpired',
                    'timeoutSeconds': 3, 'helperProgress': {'phase': 'project-imports', 'events': 1}}},
            run=mock.Mock(side_effect=hp.Failure('PRIVATE')))
        with tempfile.TemporaryDirectory(prefix='PRIVATE-') as temporary, \
             mock.patch.object(hp.sys, 'platform', 'darwin'), mock.patch.object(support, 'Runner', return_value=runner), \
             mock.patch.object(support, 'clean_environment', return_value={}), mock.patch.object(hp, 'select_abi', return_value=True), \
             contextlib.redirect_stdout(io.StringIO()) as output:
            task = Path(temporary); (task/'started').write_text('1')
            value = hp.run_experiment(task, {})
        self.assertEqual(value['firstFailure']['source'], 'runner')
        self.assertEqual(value['firstFailure']['code'], 'monitor-unavailable')
        self.assertEqual(value['monitorFailure']['queryFailure']['helperProgress']['events'], 1)
        self.assertNotIn('PRIVATE', output.getvalue())

    def test_recovered_early_timeout_history_survives_later_good_activity_without_private_metadata(self):
        import support
        query = {'operation': 'kern-proc-pgrp-inventory', 'errorType': 'TimeoutExpired',
            'timeoutSeconds': 3, 'elapsedSeconds': 3.002,
            'helperProgress': {'phase': 'project-imports', 'events': 1, 'PRIVATE': 'SECRET'},
            'argv': ['PRIVATE'], 'path': '/PRIVATE'}
        partial = {'unknownKinds': {'inventory-timeout': 1}, 'queryFailure': query,
                   'knownRSSLowerBoundMiB': None, 'formatExamples': ['PRIVATE']}
        early = {'stage': 'helper-import-workload', 'sampleComplete': True, 'sampleAttempts': 2,
            'rssMiB': 12, 'cpuSeconds': 1.5, 'partialSamples': [partial],
            'unknownKinds': {}, 'inventoryBackend': 'kern-proc-pgrp-helper', '_pids': [987654321],
            'PRIVATE': 'SECRET'}
        later = {'sampleComplete': True, 'sampleAttempts': 1, 'rssMiB': 13, 'cpuSeconds': 2,
            'partialSamples': [], 'unknownKinds': {}, 'inventoryBackend': 'kern-proc-pgrp-helper',
            'PRIVATE': 'SECRET'}
        records = [{'stage': 'helper-import-workload', 'exitCode': 0, 'groupCleanup': 'verified-gone',
            'leaderReap': {'status': 'reaped', 'exitCode': 0, 'PRIVATE': 'SECRET'}, 'cleanupFailure': None,
            'reason': None, 'failurePhase': None, 'primaryAnchor': primary_anchor(),
            'peakRSSMiB': 13, 'minimumFreeGiB': 20, 'lastActivity': later}]
        def run(_stage, args, *_args, **_kwargs):
            hp.write_json(Path(args[-1])/'workload.json', {'status': 'passed'})
        runner = SimpleNamespace(failure=None, monitor_failure=None, records=records, run=run,
            monitor_diagnostics=[{**early, 'elapsedSeconds': index+1} for index in range(16)])
        with tempfile.TemporaryDirectory(prefix='PRIVATE-') as temporary, \
             mock.patch.object(hp.sys, 'platform', 'darwin'), mock.patch.object(support, 'Runner', return_value=runner), \
             mock.patch.object(support, 'clean_environment', return_value={}), mock.patch.object(hp, 'select_abi', return_value=True), \
             contextlib.redirect_stdout(io.StringIO()) as output:
            task = Path(temporary); (task/'started').write_text('1')
            value = hp.run_experiment(task, {})
            saved = json.loads((task/'helper-import-preflight.json').read_text())
        self.assertEqual(value['status'], 'passed'); self.assertIsNone(value['firstFailure'])
        self.assertIsNone(value['monitorFailure']); self.assertEqual(saved, value)
        self.assertEqual(len(value['monitorDiagnostics']), 16)
        self.assertEqual([row['elapsedSeconds'] for row in value['monitorDiagnostics']], list(range(1, 17)))
        for row in value['monitorDiagnostics']:
            self.assertTrue(row['sampleComplete']); self.assertEqual(row['sampleAttempts'], 2)
            retained = row['partialSamples'][0]
            self.assertEqual(retained['unknownKinds'], {'inventory-timeout': 1})
            self.assertEqual(retained['queryFailure']['helperProgress'], {'phase': 'project-imports', 'events': 1})
            self.assertEqual(retained['queryFailure']['elapsedSeconds'], 3.002)
            self.assertIsNone(retained['knownRSSLowerBoundMiB'])
        self.assertEqual(value['stages'][0]['lastActivity']['partialSamples'], [])
        self.assertEqual(value['stages'][0]['lastActivity']['sampleAttempts'], 1)
        self.assertNotIn('PRIVATE', output.getvalue()); self.assertNotIn('987654321', output.getvalue())

    def test_receipt_write_failure_preserves_first_cause_and_always_emits_safe_stdout(self):
        for primary in (None, {'phase': 'monitor', 'code': 'monitor-unavailable'}):
            value = {'status': 'passed' if primary is None else 'failed', 'firstFailure': primary}
            with mock.patch.object(hp, 'write_json', side_effect=PermissionError(errno.EACCES, 'PRIVATE')), \
                 contextlib.redirect_stdout(io.StringIO()) as output:
                hp.persist(Path('/PRIVATE'), value, 'helperImportPreflight')
            self.assertEqual(value['status'], 'failed')
            self.assertEqual(value['firstFailure'], primary or {'phase': 'receipt', 'code': 'helper-import-receipt-write'})
            self.assertEqual(json.loads(output.getvalue())['helperImportPreflight'], value)
            self.assertNotIn('PRIVATE', output.getvalue())

    def test_final_driver_write_failure_still_preserves_platform_primary_and_prints_receipt(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(hp.sys, 'platform', 'linux'), \
             mock.patch.object(hp, 'write_json', side_effect=PermissionError(errno.EACCES, 'PRIVATE')), \
             contextlib.redirect_stdout(io.StringIO()) as output:
            value = hp.run_experiment(Path(temporary), {})
        self.assertEqual(value['firstFailure']['phase'], 'platform')
        self.assertEqual(value['receiptWriteFailure']['errno'], errno.EACCES)
        self.assertEqual(json.loads(output.getvalue())['helperImportPreflight'], value)
        self.assertNotIn('PRIVATE', output.getvalue())

    def test_existing_abi_is_fully_validated_not_recompiled_and_invalid_never_replaced(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()):
            task = Path(temporary); path = task/'census-abi.json'; path.write_text('PRIVATE')
            env = {}
            with mock.patch.object(hp, 'load_abi') as load, mock.patch.object(hp, 'prepare_abi') as prepare:
                self.assertTrue(hp.select_abi(task, mock.Mock(), env))
            load.assert_called_once_with(path); prepare.assert_not_called()
            self.assertEqual(env['FLOORP_CENSUS_ABI'], str(path))
            with mock.patch.object(hp, 'load_abi', side_effect=hp.Failure('PRIVATE')), \
                 mock.patch.object(hp, 'prepare_abi') as prepare, self.assertRaises(hp.Failure):
                hp.select_abi(task, mock.Mock(), {})
            prepare.assert_not_called(); self.assertEqual(path.read_text(), 'PRIVATE')

    def test_missing_abi_prepares_then_validates(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()):
            task = Path(temporary); path = task/'census-abi.json'
            with mock.patch.object(hp, 'prepare_abi', return_value=path) as prepare, mock.patch.object(hp, 'load_abi') as load:
                self.assertFalse(hp.select_abi(task, mock.Mock(), {}))
            prepare.assert_called_once(); load.assert_called_once_with(path)

    def test_linux_failure_always_writes_separate_safe_receipt_without_runner_stage(self):
        with tempfile.TemporaryDirectory(prefix='PRIVATE-') as temporary, \
             mock.patch.object(hp.sys, 'platform', 'linux'), \
             contextlib.redirect_stdout(io.StringIO()) as output:
            value = hp.run_experiment(Path(temporary), {})
            saved = json.loads((Path(temporary)/'helper-import-preflight.json').read_text())
        self.assertEqual(saved, value); self.assertEqual(value['status'], 'failed')
        self.assertEqual(value['firstFailure']['phase'], 'platform'); self.assertIsNone(value['monitorFailure'])
        self.assertEqual(value['monitorDiagnostics'], [])
        self.assertNotIn('PRIVATE', output.getvalue()); self.assertFalse(value['startupRepairEstablished'])


if __name__ == '__main__':
    unittest.main()

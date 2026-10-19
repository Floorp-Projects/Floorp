# SPDX-License-Identifier: MPL-2.0
import contextlib
import errno
import io
import json
import os
from pathlib import Path
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


class SamplingTests(unittest.TestCase):
    def test_actual_fresh_wrapper_emits_parseable_stderr_graph_for_both_variants(self):
        for variant in hp.VARIANTS:
            with self.subTest(variant=variant):
                value = subprocess.run([sys.executable, '-c', hp.WRAPPER, variant,
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
            self.assertTrue(run.call_args.kwargs['check'])
            self.assertEqual(run.call_args.args[0][2], hp.WRAPPER)
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
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()), \
             mock.patch.object(hp, 'sample', side_effect=measured), contextlib.redirect_stdout(io.StringIO()):
            result = hp.workload(Path(temporary), seconds=.25, pairs=2, workers=2, mib=1, disk_mib=1)
            self.assertEqual(result['status'], 'passed', result['firstFailure'])
            self.assertFalse((Path(temporary)/'disk-control.bin').exists())
            self.assertEqual(result['temporaryDiskRemoval']['status'], 'removed-after-exact-reap')
            self.assertEqual(result['readyWorkers'], 2); self.assertEqual(result['barriersReleased'], 2)
            self.assertTrue(all(row['reaped'] and row['exitCode'] == 0 and row['failure'] is None for row in result['cleanup']))
            self.assertTrue(all(row['sufficient'] and row['cpuIterations'] > 0 and row['elapsedSeconds'] >= .25
                                and row['exitCode'] == 0 for row in result['workerMeasurements']))
            self.assertEqual(result['workerMeasurements'][0]['diskInitialWriteBytes'], 1024**2)
            self.assertGreaterEqual(result['workerMeasurements'][0]['diskReadBytes'], 1024**2)
            self.assertGreaterEqual(result['workerMeasurements'][0]['fsyncs'], 2)
            self.assertEqual(result['workerMeasurements'][1]['diskInitialWriteBytes'], 0)
            for row in result['samples']:
                if row['phase'] == 'contended':
                    self.assertTrue(row['loadBefore']['complete']); self.assertTrue(row['loadAfter']['complete'])
            self.assertEqual([row['variant'] for row in result['samples']], list(hp.VARIANTS)*4)
            for phase in ('idle', 'contended'):
                self.assertEqual(result['counts'][phase]['lean']['validMeasurements'], 2)
            self.assertNotIn('PRIVATE', json.dumps(result)); self.assertEqual(result['osCacheState'], 'unknown-cold-or-warm')

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
             mock.patch.object(hp, 'sample', side_effect=unknown), contextlib.redirect_stdout(io.StringIO()):
            value = hp.workload(Path(temporary), seconds=.2, pairs=2, workers=2, mib=1, disk_mib=1)
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
        def insufficient(*args):
            value = original(*args); value['sufficient'] = False; return value
        original = hp.worker_measurement
        with tempfile.TemporaryDirectory() as temporary, mock.patch.dict(os.environ, environment()), \
             mock.patch.object(hp, 'sample', side_effect=measured), \
             mock.patch.object(hp, 'worker_measurement', side_effect=insufficient), \
             contextlib.redirect_stdout(io.StringIO()):
            value = hp.workload(Path(temporary), seconds=.15, pairs=2, workers=2, mib=1, disk_mib=1)
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
             mock.patch.object(hp, 'sample', side_effect=measured), \
             mock.patch.object(hp, 'load_presence', side_effect=vanished), contextlib.redirect_stdout(io.StringIO()):
            value = hp.workload(Path(temporary), seconds=.15, pairs=2, workers=2, mib=1, disk_mib=1)
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
             mock.patch.object(hp, 'sample', side_effect=measured), \
             mock.patch('inventory_stress.cleanup_children', side_effect=cleanup), \
             contextlib.redirect_stdout(io.StringIO()):
            value = hp.workload(Path(temporary), seconds=.15, pairs=2, workers=2, mib=1, disk_mib=1)
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


class DriverTests(unittest.TestCase):
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

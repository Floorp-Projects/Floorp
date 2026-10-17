# SPDX-License-Identifier: MPL-2.0
import copy
import contextlib
import ctypes
import datetime
import errno
import io
import json
import os
from pathlib import Path
import plistlib
import signal
import struct
import subprocess
import sys
import tarfile
import tempfile
import textwrap
import time
import unittest
import urllib.error
import zipfile
from types import SimpleNamespace
from unittest import mock
import native_cause
import fetch_sdk
import materialize
import monitor_native_preflight
from darwin_activity import BSDInfo, DarwinActivity, TaskAllInfo, TaskInfo
from support import Failure, ProgressWatch, Runner, clean_environment, digest, extract_tar, scan_tar
from native_suite import cause_failure_snapshot, context_result, failure_snapshot
from pipeline import macho, prepare_harness, verify_config
from owned_native import MacProcesses, OwnedNative


class Guards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def patched_host_class(self):
        fixture = Path(__file__).resolve().parents[3]
        plan = json.loads(Path(__file__).with_name('plan.json').read_text())
        harness = self.root / 'harness'
        target = harness / plan['harness']['startupPatch']['target']
        target.parent.mkdir(parents=True)
        target.write_bytes((fixture / plan['harness']['startupPatch']['target']).read_bytes())
        runner = mock.Mock()
        def run(stage, command, cwd, env, minutes):
            self.assertEqual(minutes, 360)
            subprocess.run(command, cwd=cwd, env=env, check=True, capture_output=True)
        runner.run.side_effect = run
        proof = prepare_harness(plan, harness, fixture, runner, os.environ.copy())
        self.assertEqual(proof['patchedSHA256'], plan['harness']['startupPatch']['patchedSHA256'])
        module = native_cause.load_module('startup_harness', target)
        host = module.HostProcess.__new__(module.HostProcess)
        host.launch_services, host.pid, host.identity = True, None, None
        host.exited, host.verified, host.exit_events = False, False, None
        host.exit_code, host.requested_signal = None, None
        host.process = mock.Mock()
        host.profile = self.root / 'isolated-profile'
        host.command = ['/validation.app/browser', '--profile', str(host.profile)]
        return module, host

    def test_launcher_exit_is_not_unobserved_browser_exit(self):
        _, host = self.patched_host_class()
        with mock.patch.object(host, '_process_rows', return_value=[]):
            for launcher_code in (0, 1, -9, None):
                host.process.poll.return_value = launcher_code
                self.assertIsNone(host.poll())
                self.assertIsNone(host.pid)
                self.assertFalse(host.exited)
        host.process.poll.assert_not_called()

    def test_poll_passively_adopts_exact_browser_before_session(self):
        _, host = self.patched_host_class()
        identity = (42, 'fixed birth', ' '.join(host.command))
        other = (43, 'other birth', '/another/browser --profile /another/profile')
        def adopt(value):
            host.pid, host.identity = value[0], value
        with mock.patch.object(host, '_process_rows', return_value=[other, identity]), \
                mock.patch.object(host, '_adopt', side_effect=adopt) as adopted, \
                mock.patch.object(host, '_read_exit_events'):
            self.assertIsNone(host.poll())
        adopted.assert_called_once_with(identity)
        self.assertEqual(host.pid, 42)
        self.assertFalse(host.verified)
        host.process.poll.assert_not_called()

    def test_early_browser_identity_cannot_be_rebound_by_marionette(self):
        _, host = self.patched_host_class()
        original = (42, 'first birth', ' '.join(host.command))
        host.pid, host.identity, host.exit_events = 42, original, mock.Mock()
        queue = host.exit_events
        with mock.patch.object(host, '_matching_identity') as matching, \
                mock.patch.object(host, '_adopt') as adopt:
            for replacement in ((43, 'first birth', original[2]), (42, 'later birth', original[2])):
                matching.return_value = replacement
                with self.assertRaisesRegex(RuntimeError, 'identity differs'):
                    host.verify_session({'capabilities': {'moz:processID': replacement[0],
                                                         'moz:profile': str(host.profile)}})
                self.assertEqual(host.identity, original)
                self.assertIs(host.exit_events, queue)
                self.assertFalse(host.verified)
            adopt.assert_not_called()
            matching.return_value = original
            host.verify_session({'capabilities': {'moz:processID': 42, 'moz:profile': str(host.profile)}})
            adopt.assert_called_once_with(original)
            self.assertTrue(host.verified)

    def test_observed_browser_exit_is_not_hidden_by_running_launcher(self):
        _, host = self.patched_host_class()
        host.pid, host.identity, host.exited, host.exit_code = 42, (42, 'birth', 'argv'), True, -11
        host.process.poll.return_value = None
        self.assertEqual(host.poll(), 0)
        self.assertEqual(host.returncode, -11)

    def test_no_browser_identity_cleanup_only_reaps_owned_launcher(self):
        _, host = self.patched_host_class()
        with mock.patch.object(host, '_process_rows', return_value=[]), \
                mock.patch.object(host, 'terminate') as terminate, \
                mock.patch.object(host, 'kill') as kill:
            host.close()
        terminate.assert_not_called()
        kill.assert_not_called()
        host.process.wait.assert_called_once_with(timeout=2)
        self.assertIsNone(host.identity)

    def test_startup_snapshot_keeps_codes_and_markers_without_private_text(self):
        _, host = self.patched_host_class()
        host.process.poll.return_value = 1
        (self.root / 'browser.private.log').write_text('PRIVATE path Error Domain=NSOSStatusErrorDomain\n')
        (self.root / 'browser.stderr.log').write_text('PRIVATE token Assertion failure: detail\nMOZ_CRASH detail\n')
        with mock.patch.object(host, '_process_rows', return_value=[]):
            snapshot = native_cause.startup_snapshot(host, self.root)
        self.assertEqual(snapshot['hostStartup']['browserLiveness'], 'not-observed')
        self.assertIsNone(snapshot['hostStartup']['browserExitCode'])
        self.assertEqual(snapshot['hostStartup']['launcherExitCode'], 1)
        self.assertIn('gecko-fatal-assertion', snapshot['streams']['stderr']['markers'])
        self.assertNotIn('PRIVATE', json.dumps(snapshot))
        snapshot['hostStartup']['argv'] = 'PRIVATE'
        snapshot['streams']['stderr']['markers'].append('PRIVATE')
        snapshot['streams']['stderr']['rawContent'] = 'PRIVATE'
        projected = cause_failure_snapshot({'status': 'failed', 'beforeCleanup': snapshot})
        self.assertNotIn('PRIVATE', json.dumps(projected))
        self.assertEqual(projected['beforeCleanupHostStartup']['launcherExitCode'], 1)

    def test_snapshot_failure_does_not_skip_cleanup_or_replace_first_failure(self):
        browser = self.root / 'validation.app'
        (browser / 'Contents').mkdir(parents=True)
        (browser / 'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': 'browser'}))
        output = self.root / 'cause'
        host = mock.Mock(pid=42, identity=(42, 'birth', 'argv'), verified=False)
        host.poll.side_effect = RuntimeError('PRIVATE observation')
        harness, lifecycle = mock.Mock(), mock.Mock()
        harness.HostProcess.return_value = host
        harness.unused_port.return_value = 12345
        harness.wait_for.side_effect = lambda description, callback, timeout: callback()
        argv = ['native_cause.py', '--browser', str(browser), '--harness', str(self.root),
                '--output', str(output), '--build-id', '20261017000000']
        with mock.patch.object(sys, 'argv', argv), \
                mock.patch('native_cause.load_module', side_effect=[harness, lifecycle]):
            self.assertEqual(native_cause.main(), 1)
        host.close.assert_called_once()
        result = json.loads((output / 'result.json').read_text())
        self.assertEqual(result['errorCode'], 'RuntimeError')
        self.assertEqual(result['status'], 'failed')
        self.assertTrue(result['diagnosticsIncomplete'])
        self.assertFalse(result['cleanupComplete'])
        self.assertNotIn('PRIVATE', json.dumps(result))
        self.assertEqual(harness.wait_for.call_args.args[2], 35)

    def test_snapshot_liveness_and_exit_code_use_the_same_kernel_observation(self):
        module, host = self.patched_host_class()
        host.pid, host.identity, host.verified = 42, (42, 'birth', 'argv'), True
        host.process.poll.return_value = None
        def observed_exit():
            host.exited, host.exit_code = True, -11
        with mock.patch.object(host, '_read_exit_events', side_effect=observed_exit), \
                mock.patch.object(module.HostProcess, 'returncode', new_callable=mock.PropertyMock) as code:
            snapshot = native_cause.startup_snapshot(host, self.root)
        code.assert_not_called()
        self.assertEqual(snapshot['hostStartup']['browserLiveness'], 'exit-observed')
        self.assertEqual(snapshot['hostStartup']['browserExitCode'], -11)

    def test_startup_stream_codes_keep_first_and_last_failure_without_raw_text(self):
        _, host = self.patched_host_class()
        host.process.poll.return_value = 1
        first = ('PRIVATE Error Domain=NSOSStatusErrorDomain Code=-10827\n'
                 'PRIVATE Error Domain=NSOSStatusErrorDomain Code=-2147418113\n')
        last = '\nPRIVATE Error Domain=NSOSStatusErrorDomain Code=-10673\n'
        (self.root / 'browser.stderr.log').write_text(first + ('padding\n' * 15000) + last)
        with mock.patch.object(host, '_process_rows', return_value=[]):
            snapshot = native_cause.startup_snapshot(host, self.root)
        stream = snapshot['streams']['stderr']
        self.assertEqual(stream['osStatusCodes'], [-10827, -2147418113, -10673])
        self.assertTrue(stream['tailTruncated'])
        self.assertLessEqual(stream['headBytesRead'] + stream['tailBytesRead'], 65536)
        projected = cause_failure_snapshot({'status': 'failed', 'beforeCleanup': snapshot})
        self.assertEqual(projected['beforeCleanupStreams']['stderr']['osStatusCodes'], [-10827, -2147418113, -10673])
        self.assertNotIn('PRIVATE', json.dumps(projected))
        self.assertEqual(projected['applicationEnvironmentBeforeMarionette'], 'UNVERIFIED_LAUNCHSERVICES_ENVIRONMENT')

    def test_cleanup_requested_signal_is_distinct_from_observed_browser_signal(self):
        _, host = self.patched_host_class()
        host.exit_code, host.requested_signal = -11, signal.SIGTERM
        host.process.poll.return_value = 0
        value = native_cause.exit_snapshot(host)
        self.assertEqual(value['browserSignal'], 11)
        self.assertEqual(value['requestedSignal'], 15)
        self.assertTrue(value['browserExitStatusAvailable'])

    def test_host_crash_during_cleanup_cannot_pass_authenticated_scenario(self):
        browser, output = self.root / 'validation.app', self.root / 'cause'
        (browser / 'Contents').mkdir(parents=True)
        (browser / 'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': 'browser'}))
        host = SimpleNamespace(pid=42, identity=(42, 'birth', 'argv'), verified=False,
            launch_services=True, exited=False, exit_code=None, requested_signal=None,
            process=mock.Mock(), command=['browser', '--no-remote', '--profile', 'PRIVATE',
                                         '--marionette', '--remote-allow-system-access', 'about:blank'])
        host.process.poll.return_value = 0
        host.poll = lambda: 0 if host.exited else None
        host.close = mock.Mock()
        host.verify_session = lambda session: setattr(host, 'verified', True)
        monitor = SimpleNamespace(status='monitoring', pid=43, start=mock.Mock(), close=mock.Mock())
        client = mock.Mock(session={'capabilities': {}})
        protections = {'debug': True, 'level': 3, 'userOverride': False, 'disableEnv': '',
                       'buildID': '20261017000000', 'version': '158.0.1', 'os': 'Darwin', 'is64Bit': True}
        events = [{'type': 'connected', 'payload': {'pid': 43}},
                  {'type': 'disconnected', 'time': 110, 'payload': {'pid': 43,
                   'reason': 'process-exited', 'causeReason': 'launch-cancelled',
                   'causeTimeMs': 105, 'connectedBeforeDisconnect': True}}]
        def script(source, *args):
            if 'AppConstants' in source: return protections
            if 'hostIdentityJSON' in source: return '{}'
            if source.endswith('.events;'): return events
            if source == 'return Date.now();': return 100
            if source.endswith('.count;'): return 2
            if '.cancelLaunch(' in source: monitor.status = 'exit-observed'
            if 'removeObserver' in source:
                host.exited, host.exit_code = True, -11
            return None
        client.script.side_effect = script
        harness, lifecycle = mock.Mock(SERVICE='service'), mock.Mock()
        harness.HostProcess.return_value = host
        harness.Marionette.return_value = client
        harness.unused_port.return_value = 12345
        harness.seal_bundle.return_value = self.root / 'shim.app'
        harness.wait_for.side_effect = lambda description, callback, timeout: callback()
        lifecycle.ShimMonitor.return_value = monitor
        argv = ['native_cause.py', '--browser', str(browser), '--harness', str(self.root),
                '--output', str(output), '--build-id', '20261017000000']
        with mock.patch.object(sys, 'argv', argv), \
                mock.patch('native_cause.load_module', side_effect=[harness, lifecycle]):
            self.assertEqual(native_cause.main(), 1)
        value = json.loads((output / 'result.json').read_text())
        self.assertEqual(value['phase'], 'complete')
        self.assertEqual(value['beforeCleanup']['hostStartup']['browserLiveness'], 'alive')
        self.assertTrue(value['cleanupComplete'])
        self.assertEqual(value['status'], 'failed')
        self.assertEqual(value['errorCode'], 'cause-host-abnormal-exit-during-cleanup')
        self.assertEqual(value['afterCleanupHost']['browserSignal'], 11)
        self.assertIsNone(value['afterCleanupHost']['requestedSignal'])
        self.assertNotIn('PRIVATE', json.dumps(cause_failure_snapshot(value)))

    def test_effective_config_uses_upstream_host_cpu_arch(self):
        tools, sdk = self.root / 'tools', self.root / 'sdk'
        substs = {'TARGET_CPU': 'x86_64', 'HOST_CPU_ARCH': 'x86_64', 'OS_ARCH': 'Darwin',
                  'OS_TARGET': 'Darwin', 'CC_TYPE': 'clang', 'CC_VERSION': '22.1.8',
                  'MOZ_APP_VERSION': '158.0.1', 'MOZ_DEBUG': '1', 'ENABLE_TESTS': '1',
                  'MOZ_SANDBOX': '1',
                  'MOZ_APP_DISPLAYNAME': 'Floorp', 'MOZ_CHILD_PROCESS_NAME': 'plugin-container',
                  'MOZ_CHILD_PROCESS_BUNDLENAME': 'plugin-container.app'}
        for key, path in (('CC', 'clang/bin/clang'), ('CXX', 'clang/bin/clang++'),
                          ('HOST_CC', 'clang/bin/clang'), ('HOST_CXX', 'clang/bin/clang++'),
                          ('RUSTC', 'rust/bin/rustc'), ('CARGO', 'rust/bin/cargo'),
                          ('NODEJS', 'node/bin/node'), ('WASI_SYSROOT', 'sysroot-wasm32-wasi'),
                          ('ONNX_RUNTIME', 'onnxruntime-x86_64-apple-darwin/libonnxruntime.dylib')):
            substs[key] = str(tools / path)
            if key in ('CC', 'CXX', 'HOST_CC', 'HOST_CXX'):
                substs[key] = [substs[key], '-mmacosx-version-min=10.15']
        substs['MACOS_SDK_DIR'] = str(sdk)
        (self.root / 'obj').mkdir()
        (self.root / 'obj/mozinfo.json').write_text(json.dumps(
            {'debug': True, 'pgo': False, 'artifact': False}))
        plan = {'criteria': {'macOSMin': '10.15'}}
        for state in ('valid', 'missing', 'wrong-architecture', 'missing-target-flag',
                      'wrong-target-flag', 'conflicting-target-flags'):
            with self.subTest(state=state):
                current = dict(substs)
                if state == 'missing':
                    current.pop('HOST_CPU_ARCH')
                elif state == 'wrong-architecture':
                    current['HOST_CPU_ARCH'] = 'aarch64'
                elif state == 'missing-target-flag':
                    current['CC'] = current['CC'][:1]
                elif state == 'wrong-target-flag':
                    current['CXX'] = current['CXX'][:1] + ['-mmacosx-version-min=11.0']
                elif state == 'conflicting-target-flags':
                    current['HOST_CC'] = current['HOST_CC'] + ['-mmacosx-version-min=11.0']
                (self.root / 'effective-config.private.log').write_text(json.dumps({'substs': current}))
                if state == 'valid':
                    verify_config(plan, self.root, tools, sdk)
                    record = json.loads((self.root / 'effective-config.json').read_text())
                    self.assertEqual(record['HOST_CPU_ARCH'], 'x86_64')
                    self.assertNotIn('HOST_CPU', record)
                    self.assertEqual(record['macOSMinimumFromFourCompilerFlags'], '10.15')
                    self.assertNotIn('MACOSX_DEPLOYMENT_TARGET', record)
                    self.assertNotIn('CARGO_VERSION', record)
                else:
                    code = ('compiled-deployment-target-' if 'flag' in state else
                            'compiled-config-host_cpu_arch')
                    with self.assertRaisesRegex(Failure, code):
                        verify_config(plan, self.root, tools, sdk)

    def test_process_activity_parses_darwin_and_linux_cpu_formats(self):
        output = '900 901 2048 0:01.25 S\n900 902 1024 01:00:00 R+\n900 903 1024 1-01:00:00 S\n901 999 999999 10:00 R\n'
        with mock.patch('support.sys.platform', 'linux'), \
                mock.patch('support.subprocess.run', return_value=mock.Mock(stdout=output)):
            value = Runner.group_activity(900)
        self.assertEqual(value['cpuSeconds'], 93601.25)
        self.assertEqual(value['rssMiB'], 4)
        self.assertEqual(value['processes'], 3)
        self.assertEqual(value['states'], ['R', 'S'])

    def test_quiet_link_with_cpu_progress_is_not_a_stall(self):
        watch = ProgressWatch(20)
        activity = {'cpuSeconds': 1, 'rssMiB': 10, '_pids': (1,), 'processes': 1, 'states': ['S']}
        watch.observe(0, 0, 64 * 1024**3, activity)
        activity['cpuSeconds'] = 3600
        quiet, stalled = watch.observe(7200, 0, 64 * 1024**3, activity)
        self.assertEqual(quiet, 0)
        self.assertFalse(stalled)

    def test_unknown_darwin_process_state_is_partial_and_resampled(self):
        unknown = '900 901 2048 0:01.25 S\n900 902 0 0:00.00 ?E\n'
        complete = '900 901 2048 0:01.30 S\n'
        value = Runner.parse_group_activity(unknown, 900)
        self.assertFalse(value['sampleComplete'])
        self.assertEqual(value['unknownKinds'], {'state-unavailable': 1})
        self.assertIsNone(value['rssMiB'])
        self.assertIsNone(value['cpuSeconds'])
        self.assertEqual(value['rssLowerBoundMiB'], 2)
        self.assertEqual(value['cpuLowerBoundSeconds'], 1.25)
        self.assertIn('?', value['states'])
        with mock.patch('support.sys.platform', 'linux'), \
                mock.patch('support.subprocess.run', side_effect=[mock.Mock(stdout=unknown), mock.Mock(stdout=complete)]), \
                mock.patch('support.time.sleep'):
            recovered = Runner.group_activity(900)
        self.assertTrue(recovered['sampleComplete'])
        self.assertEqual(recovered['sampleAttempts'], 2)
        self.assertEqual(recovered['partialSamples'][0]['unknownKinds'], {'state-unavailable': 1})

    def test_missing_activity_columns_and_values_are_not_zero(self):
        for line, kind in [('900 901 0 0:00.00', 'column-count'),
                           ('900 901 ? 0:00.00 S', 'rss-unavailable'),
                           ('900 901 2048 ? S', 'cpu-unavailable')]:
            with self.subTest(kind=kind):
                value = Runner.parse_group_activity(line, 900)
                self.assertFalse(value['sampleComplete'])
                self.assertIsNone(value['rssMiB'])
                self.assertIsNone(value['cpuSeconds'])
                self.assertEqual(value['unknownKinds'], {kind: 1})
                self.assertEqual(value['processes'], 1)

    def test_persistent_partial_observation_is_bounded(self):
        with mock.patch('support.sys.platform', 'linux'), \
                mock.patch('support.subprocess.run', return_value=mock.Mock(stdout='900 901 0 0:00.00 ?\n')) as command, \
                mock.patch('support.time.sleep'):
            value = Runner.group_activity(900)
        self.assertFalse(value['sampleComplete'])
        self.assertEqual(command.call_count, 3)
        self.assertEqual(value['sampleAttempts'], 3)

    def test_monitor_preflight_requires_a_complete_real_sample(self):
        template = Path(__file__).with_name('workflow.yml.in').read_text()
        block = template.split('- name: Verify owned process sampling before source checkout\n', 1)[1]
        block = block.split('      - name: Checkout immutable Runtime source', 1)[0]
        script = textwrap.dedent(block.split("<<'PY'\n", 1)[1].rsplit('          PY', 1)[0])
        workspace = self.root / 'workspace'
        workspace.mkdir()
        (workspace / 'fixture').symlink_to(Path(__file__).resolve().parents[3], target_is_directory=True)
        (self.root / 'started').write_text(str(time.time()))
        (self.root / 'started-monotonic').write_text(str(time.monotonic()))
        native = mock.Mock(numer=1, denom=1)
        native.read.return_value = {'status': 'measured', 'rssMiB': 10, 'cpuSeconds': 1, 'state': 'R'}
        def record_zombie_only(runner, stage, *_args):
            if stage == 'monitor-zombie-preflight':
                runner.records.append({'stage': stage, 'lastActivity': {
                    'sampleComplete': True, 'counterSource': 'libproc-taskallinfo'}})
        with mock.patch.dict(os.environ, {'GITHUB_WORKSPACE': str(workspace), 'TASK_ROOT': str(self.root)}), \
                mock.patch('darwin_activity.probe', return_value=native), \
                mock.patch.object(Runner, 'run', new=record_zombie_only), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(Failure, 'monitor-preflight-no-complete-sample'):
                exec(compile(script, 'workflow-monitor-preflight', 'exec'), {})
        proof = json.loads((self.root / 'monitor-preflight.json').read_text())
        self.assertEqual(proof['status'], 'failed')
        self.assertEqual(proof['firstFailure']['code'], 'monitor-preflight-no-complete-sample')

    def test_unavailable_monitor_preserves_failure_and_reclaims_owned_group(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        runner = Runner(self.root, time.time(), limits)
        for index in range(20):
            runner.record_monitor('old-' + str(index), {'sampleComplete': True})
        incomplete = {'sampleComplete': False, 'sampleAttempts': 3,
                      'partialSamples': [{'unknownKinds': {'state-unavailable': 1}}],
                      'unknownKinds': {'state-unavailable': 1}}
        with mock.patch('support.shutil.disk_usage', return_value=mock.Mock(free=64 * 1024**3)), \
                mock.patch.object(runner, 'group_activity', return_value=incomplete):
            with self.assertRaisesRegex(Failure, 'monitor-unavailable'):
                runner.run('monitor-test', [sys.executable, '-c', 'import time; time.sleep(3)'],
                           self.root, os.environ.copy(), 360)
        self.assertEqual(runner.failure['code'], 'monitor-unavailable')
        self.assertEqual(json.loads((self.root / 'first-failure.json').read_text())['code'], 'monitor-unavailable')
        self.assertFalse(runner.groups)
        self.assertEqual(len(runner.monitor_diagnostics), 16)
        self.assertEqual(runner.monitor_diagnostics[0]['stage'], 'old-5')
        self.assertEqual(runner.monitor_diagnostics[-1]['unknownKinds'], {'state-unavailable': 1})
        fatal = copy.deepcopy(runner.monitor_failure)
        for index in range(20):
            runner.record_monitor('later-' + str(index), {'sampleComplete': True})
        self.assertEqual(runner.monitor_failure, fatal)
        self.assertEqual(runner.records[0]['lastActivity'], incomplete)
        self.assertEqual(runner.records[0]['failurePhase'], 'group-activity')
        self.assertEqual(runner.records[0]['reason'], 'monitor-unavailable')
        self.assertIsNone(runner.records[0]['peakRSSMiB'])
        self.assertIsNotNone(runner.records[0]['exitCode'])
        self.assertEqual(runner.records[0]['groupCleanup'], 'verified-gone')

    def test_sampler_exception_records_stage_and_reaps_owned_process(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        runner = Runner(self.root, time.time(), limits)
        children, popen = [], subprocess.Popen
        def launch(*args, **kwargs):
            child = popen(*args, **kwargs)
            children.append(child)
            return child
        with mock.patch('support.shutil.disk_usage', return_value=mock.Mock(free=64 * 1024**3)), \
                mock.patch('support.subprocess.Popen', side_effect=launch), \
                mock.patch.object(runner, 'group_activity', side_effect=subprocess.TimeoutExpired('private command', 3)):
            with self.assertRaises(subprocess.TimeoutExpired):
                runner.run('sampler-exception', [sys.executable, '-c', 'import time; time.sleep(30)'],
                           self.root, os.environ.copy(), 360)
        self.assertIsNotNone(children[0].poll())
        self.assertFalse(runner.group_alive(children[0].pid))
        self.assertEqual(len(runner.records), 1)
        self.assertEqual(runner.records[0]['reason'], 'TimeoutExpired')
        self.assertEqual(runner.records[0]['failurePhase'], 'group-activity')
        self.assertEqual(runner.monitor_failure['errorType'], 'TimeoutExpired')
        self.assertNotIn('private command', json.dumps(runner.records + runner.monitor_diagnostics))

    def test_cleanup_failure_keeps_initial_failure_and_final_stage_record(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        runner = Runner(self.root, time.time(), limits)
        runner.native_registry = mock.Mock()
        runner.native_registry.finish.return_value = {'outlivedHarness': 1, 'allOwnedGone': False}
        incomplete = {'sampleComplete': False, 'rssMiB': None, 'rssLowerBoundMiB': 0}
        with mock.patch('support.shutil.disk_usage', return_value=mock.Mock(free=64 * 1024**3)), \
                mock.patch.object(runner, 'group_activity', return_value=incomplete):
            with self.assertRaisesRegex(Failure, 'owned-native-cleanup-incomplete'):
                runner.run('cleanup-failure', [sys.executable, '-c', 'import time; time.sleep(30)'],
                           self.root, os.environ.copy(), 360)
        self.assertEqual(runner.failure['code'], 'monitor-unavailable')
        self.assertEqual(runner.records[0]['reason'], 'monitor-unavailable')
        self.assertEqual(runner.records[0]['cleanupFailure'], 'owned-native-cleanup-incomplete')
        self.assertIsNone(runner.records[0]['peakRSSMiB'])
        self.assertFalse(runner.groups)

    def test_launch_failure_still_has_a_stage_record(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        runner = Runner(self.root, time.time(), limits)
        with self.assertRaises(FileNotFoundError):
            runner.run('launch-failure', [str(self.root / 'missing-executable')],
                       self.root, os.environ.copy(), 360)
        self.assertEqual(len(runner.records), 1)
        self.assertEqual(runner.records[0]['failurePhase'], 'launch')
        self.assertIsNone(runner.records[0]['exitCode'])
        self.assertIsNone(runner.records[0]['peakRSSMiB'])

    def test_group_permission_failure_still_reaps_exact_child_and_keeps_denial(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        incomplete = {'sampleComplete': False, 'rssMiB': None, 'rssLowerBoundMiB': 0}
        for denied_signal, operation in ((0, 'group-probe'), (signal.SIGINT, 'group-signal')):
            with self.subTest(denied_signal=denied_signal), tempfile.TemporaryDirectory() as directory:
                runner = Runner(directory, time.time(), limits)
                children, popen = [], subprocess.Popen
                def launch(*args, **kwargs):
                    child = popen(*args, **kwargs)
                    children.append(child)
                    return child
                def killpg(_pid, sig):
                    if sig == denied_signal:
                        raise PermissionError(errno.EPERM, 'private denied process identity')
                with mock.patch('support.shutil.disk_usage', return_value=mock.Mock(free=64 * 1024**3)), \
                        mock.patch('support.subprocess.Popen', side_effect=launch), \
                        mock.patch.object(runner, 'group_activity', return_value=incomplete), \
                        mock.patch('support.os.killpg', side_effect=killpg):
                    with self.assertRaises(PermissionError):
                        runner.run('denied-cleanup', [sys.executable, '-c',
                                   'import time; time.sleep(0.1); raise SystemExit(7)'],
                                   directory, os.environ.copy(), 360)
                record = runner.records[0]
                self.assertEqual(runner.failure['code'], 'monitor-unavailable')
                self.assertEqual(record['reason'], 'monitor-unavailable')
                self.assertEqual(record['cleanupFailure'], 'PermissionError')
                self.assertEqual(record['groupCleanup'], 'unverified')
                self.assertEqual(record['cleanupDiagnostics'][-1], {
                    'operation': operation, 'signal': int(denied_signal),
                    'errorType': 'PermissionError', 'errno': errno.EPERM})
                self.assertEqual(record['leaderReap'], {'status': 'reaped', 'exitCode': 7})
                self.assertEqual(record['exitCode'], 7)
                self.assertIsNone(record['peakRSSMiB'])
                with self.assertRaises(ChildProcessError):
                    os.waitpid(children[0].pid, os.WNOHANG)
                self.assertNotIn('private denied', json.dumps(record))

    def test_denied_group_and_live_child_remain_unverified_after_bounded_wait(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        runner = Runner(self.root, time.time(), limits)
        child = mock.Mock(pid=99999, returncode=None)
        child.poll.return_value = None
        child.wait.side_effect = subprocess.TimeoutExpired('private argv', 5)
        incomplete = {'sampleComplete': False, 'rssMiB': None, 'rssLowerBoundMiB': 0}
        with mock.patch('support.shutil.disk_usage', return_value=mock.Mock(free=64 * 1024**3)), \
                mock.patch('support.subprocess.Popen', return_value=child), \
                mock.patch.object(runner, 'group_activity', return_value=incomplete), \
                mock.patch('support.os.killpg', side_effect=PermissionError(errno.EPERM, 'denied')):
            with self.assertRaises(PermissionError):
                runner.run('denied-live', ['unused'], self.root, os.environ.copy(), 360)
        self.assertEqual(runner.records[0]['leaderReap'], {'status': 'still-running', 'exitCode': None})
        self.assertEqual(runner.records[0]['cleanupFailure'], 'PermissionError')
        self.assertEqual(runner.records[0]['groupCleanup'], 'unverified')
        self.assertIsNone(runner.records[0]['exitCode'])
        child.wait.assert_called_once_with(timeout=5)
        self.assertEqual(runner.failure['code'], 'monitor-unavailable')

    def test_stall_requires_multiple_stable_signals(self):
        activity = {'cpuSeconds': 1, 'rssMiB': 10, '_pids': (1,), 'processes': 1, 'states': ['S']}
        for changed in ('none', 'log', 'cpu', 'disk', 'rss', 'process', 'runnable'):
            with self.subTest(changed=changed):
                watch = ProgressWatch(20)
                watch.observe(0, 0, 64 * 1024**3, activity)
                current, log, free = dict(activity), 0, 64 * 1024**3
                if changed == 'log': log = 100
                elif changed == 'cpu': current['cpuSeconds'] = 2
                elif changed == 'disk': free -= 16 * 1024**2
                elif changed == 'rss': current['rssMiB'] = 12
                elif changed == 'process': current['_pids'] = (1, 2)
                elif changed == 'runnable': current['states'] = ['R']
                _, stalled = watch.observe(1201, log, free, current)
                self.assertEqual(stalled, changed == 'none')

    def test_elapsed_policy_uses_platform_limit_without_early_phase_deadline(self):
        plan = json.loads(Path(__file__).with_name('plan.json').read_text())
        limits = plan['limits']
        self.assertEqual(limits['jobMinutes'], 360)
        for key, value in limits.items():
            if key.endswith('DeadlineMinutes'):
                self.assertEqual(value, 360)
        self.assertIn('timeout-minutes: 360', Path(__file__).with_name('workflow.yml.in').read_text())
        (self.root / 'started-monotonic').write_text(str(time.monotonic() - 211 * 60))
        runner = Runner(self.root, time.time() - 211 * 60, limits)
        activity = {'cpuSeconds': 1, 'rssMiB': 10, '_pids': (1,), 'processes': 1, 'states': ['S'],
                    'sampleComplete': True}
        with mock.patch('support.shutil.disk_usage', return_value=mock.Mock(free=64 * 1024**3)), \
                mock.patch.object(runner, 'group_activity', return_value=activity):
            runner.run('healthy-long-job', [sys.executable, '-c', 'import time; time.sleep(0.02)'],
                       self.root, os.environ.copy(), 360)
        self.assertIsNone(runner.failure)
        with self.assertRaisesRegex(Failure, 'platform-budget-binding'):
            runner.run('old-budget', [sys.executable, '-c', 'pass'], self.root, os.environ.copy(), 185)

    def test_slow_cumulative_io_progress_resets_stall_window(self):
        watch = ProgressWatch(20)
        activity = {'cpuSeconds': 1, 'rssMiB': 10, '_pids': (1,), 'processes': 1, 'states': ['S']}
        free = 64 * 1024**3
        watch.observe(0, 0, free, activity)
        watch.observe(600, 0, free - 4 * 1024**2, activity)
        watch.observe(1199, 0, free - 8 * 1024**2, activity)
        quiet, stalled = watch.observe(1201, 0, free - 8 * 1024**2, activity)
        self.assertEqual(quiet, 2)
        self.assertFalse(stalled)

    def archive(self, entries):
        path = self.root / 'input.tar.gz'
        with tarfile.open(path, 'w:gz') as archive:
            for name, kind, data in entries:
                item = tarfile.TarInfo(name)
                item.type = kind
                if kind == tarfile.REGTYPE:
                    item.size = len(data)
                    archive.addfile(item, io.BytesIO(data))
                else:
                    item.linkname = data
                    archive.addfile(item)
        return path

    def test_reject_escaping_links_and_special_files(self):
        for kind, name, target in [(tarfile.SYMTYPE, 'SDK/link', '../../escape'),
                                    (tarfile.LNKTYPE, 'SDK/link', '../escape'),
                                    (tarfile.REGTYPE, '/outside', b'x'),
                                    (tarfile.FIFOTYPE, 'pipe', '')]:
            with self.subTest(name=name, kind=kind), self.assertRaises(Failure):
                scan_tar(self.archive([(name, kind, target)]))

    def test_reject_archive_ambiguity_and_size(self):
        with self.assertRaises(Failure):
            scan_tar(self.archive([('a', tarfile.REGTYPE, b'a'), ('./a', tarfile.REGTYPE, b'b')]))
        with self.assertRaises(Failure):
            scan_tar(self.archive([('a', tarfile.REGTYPE, b'ab')]), max_bytes=1)

    def test_extract_internal_framework_link_and_hardlink(self):
        path = self.archive([('SDK/Versions/A/file', tarfile.REGTYPE, b'checked'),
                             ('SDK/Versions/Current', tarfile.SYMTYPE, 'A'),
                             ('SDK/copy', tarfile.LNKTYPE, 'SDK/Versions/A/file')])
        extract_tar(path, self.root / 'output')
        self.assertEqual((self.root / 'output/SDK/Versions/Current/file').read_bytes(), b'checked')
        self.assertEqual((self.root / 'output/SDK/copy').read_bytes(), b'checked')

    def test_failure_cannot_be_replaced_by_cleanup_failure(self):
        first = Runner(self.root, time.time(), {})
        first.fail('compile', 'initial')
        later = Runner(self.root, time.time(), {})
        later.fail('cleanup', 'secondary')
        self.assertEqual(first.failure, later.failure)
        self.assertEqual(json.loads((self.root / 'first-failure.json').read_text())['stage'], 'compile')

    def test_first_failure_survives_evidence_storage_error(self):
        runner = Runner(self.root, time.time(), {})
        with mock.patch('support.os.link', side_effect=OSError(errno.ENOSPC, 'no space')):
            runner.fail('compile', 'initial')
        self.assertEqual(runner.failure['code'], 'initial')
        self.assertEqual(runner.failure['persistenceError'], errno.ENOSPC)

    def test_group_cleanup_does_not_stop_at_leader_exit(self):
        runner = Runner(self.root, time.time(), {'processCleanupGraceSeconds': 0})
        alive, signals = [True], []
        def killpg(_pid, sig):
            if not alive[0]:
                raise ProcessLookupError()
            if sig:
                signals.append(sig)
                alive[0] = False
        with mock.patch('support.os.killpg', side_effect=killpg):
            runner.stop(mock.Mock(pid=99999, poll=mock.Mock(return_value=0)))
        self.assertTrue(signals)
        self.assertFalse(alive[0])

    def test_deadline_uses_monotonic_origin(self):
        (self.root / 'started-monotonic').write_text(str(time.monotonic() - 10))
        runner = Runner(self.root, 0, {})
        with mock.patch('support.time.time', return_value=-10**10):
            self.assertGreaterEqual(runner.elapsed(), 10)
            self.assertLess(runner.elapsed(), 11)

    def owned_registry(self, current):
        registry = OwnedNative.__new__(OwnedNative)
        registry.task = self.root
        registry.finished, registry.proof = False, None
        original = {'pid': 99999, 'uid': os.geteuid(), 'startSec': 100, 'startUsec': 10,
                    'exe': '/private/test.app/browser', 'argvSHA256': 'a' * 64, 'kind': 'host'}
        registry.records = {(99999, 100, 10): original}
        registry.system = mock.Mock()
        registry.system.describe.side_effect = lambda _pid: current[0]
        registry.census = mock.Mock()
        return registry, original

    def test_native_cleanup_never_signals_reused_pid(self):
        current = [None]
        registry, original = self.owned_registry(current)
        current[0] = {**original, 'startUsec': 11}
        with mock.patch('owned_native.os.kill') as send:
            result = registry.finish()
        send.assert_not_called()
        self.assertTrue(result['allOwnedGone'])

    def test_native_cleanup_signals_only_matching_owned_generation(self):
        current = [None]
        registry, original = self.owned_registry(current)
        current[0] = original
        def send(pid, _sig):
            self.assertEqual(pid, original['pid'])
            current[0] = None
        with mock.patch('owned_native.os.kill', side_effect=send), mock.patch('owned_native.PASSIVE_SETTLE_SECONDS', 0):
            result = registry.finish()
        self.assertEqual(result['outlivedHarness'], 1)
        self.assertTrue(result['allOwnedGone'])

    def test_changed_arguments_of_same_generation_remain_unresolved(self):
        current = [None]
        registry, original = self.owned_registry(current)
        current[0] = {**original, 'argvSHA256': 'b' * 64}
        with mock.patch('owned_native.os.kill') as send:
            result = registry.finish()
        send.assert_not_called()
        self.assertFalse(result['allOwnedGone'])
        self.assertIn('owned-generation-identity-changed', result['errors'])

    def test_census_error_does_not_skip_known_instance_cleanup(self):
        current = [None]
        registry, original = self.owned_registry(current)
        current[0] = original
        registry.census.side_effect = Failure('unrecognized-helper')
        def send(_pid, _sig):
            current[0] = None
        with mock.patch('owned_native.os.kill', side_effect=send) as signal_owned, mock.patch('owned_native.PASSIVE_SETTLE_SECONDS', 0):
            result = registry.finish()
        signal_owned.assert_called_once()
        self.assertFalse(result['censusComplete'])
        self.assertFalse(result['allOwnedGone'])

    def test_native_census_binds_profile_and_never_persists_nonce(self):
        browser = self.root / 'validation.app'
        (browser / 'Contents').mkdir(parents=True)
        (browser / 'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': 'browser'}))
        private = self.root / 'native-private/cause'
        profile = private / 'profile'
        profile.mkdir(parents=True)
        shim = private / 'Runtime Smoke.app/Contents/MacOS/floorp-app-shim'
        shim.parent.mkdir(parents=True)
        (shim.parents[1] / 'Info.plist').write_bytes(plistlib.dumps({
            'FloorpAppShimHostPath': str(browser), 'FloorpAppShimProfilePath': str(profile)}))
        exe = browser / 'Contents/MacOS/browser'
        host_args = str(exe) + ' --no-remote --profile ' + str(profile) + ' --marionette --remote-allow-system-access about:blank'
        nonce = 'a' * 64
        shim_args = str(shim) + ' --host-service test --host-pid 42 --launch-token ' + nonce
        rows = {42: {'pid': 42, 'uid': os.geteuid(), 'startSec': 100, 'startUsec': 1, 'pgid': 1,
                     'exe': str(exe), 'argv': host_args, 'argvSHA256': '1' * 64},
                43: {'pid': 43, 'uid': os.geteuid(), 'startSec': 100, 'startUsec': 2, 'pgid': 1,
                     'exe': str(shim), 'argv': shim_args, 'argvSHA256': '2' * 64}}
        helper_relative = 'Contents/MacOS/plugin-container.app/Contents/MacOS/plugin-container'
        helper = browser / helper_relative
        rows[44] = {'pid': 44, 'uid': os.geteuid(), 'startSec': 100, 'startUsec': 3, 'pgid': 1,
                    'ppid': 42, 'exe': str(helper), 'argv': str(helper) + ' -contentproc', 'argvSHA256': '3' * 64}
        system = mock.Mock()
        system.pids.side_effect = lambda: list(rows)
        system.path.side_effect = lambda pid: rows[pid]['exe']
        system.describe.side_effect = lambda pid: rows.get(pid)
        with mock.patch('owned_native.MacProcesses', return_value=system):
            registry = OwnedNative(self.root, browser, self.root, 90, (helper_relative,))
        registry.census()
        self.assertEqual(len(registry.records), 3)
        self.assertNotIn(nonce, (self.root / 'owned-native.private.json').read_text())
        rows[44]['ppid'] = 1
        del rows[42]
        registry.census()
        self.assertIn(44, [row['pid'] for row in registry.live()])
        (shim.parents[1] / 'Info.plist').write_bytes(plistlib.dumps({
            'FloorpAppShimHostPath': str(browser), 'FloorpAppShimProfilePath': str(private / 'other')}))
        with self.assertRaises(Failure):
            registry.census()

    def test_native_child_environment_excludes_credentials_and_overrides(self):
        supplied = {'HOME': '/actual-user', 'SDK_READ_TOKEN': 'secret', 'GITHUB_TOKEN': 'secret',
                    'ACTIONS_RUNTIME_TOKEN': 'secret', 'MOZ_DISABLE_CONTENT_SANDBOX': '1',
                    'DYLD_LIBRARY_PATH': '/override', 'SSH_AUTH_SOCK': '/agent'}
        with mock.patch.dict(os.environ, supplied, clear=True):
            result = clean_environment(self.root)
        self.assertEqual(result['HOME'], '/actual-user')
        self.assertFalse(set(supplied) - {'HOME'} & set(result))

    def test_skip_untrusted_event_or_missing_exit_cannot_pass(self):
        value = {'status': 'passed', 'requestedIterations': 20, 'attemptedIterations': 20,
                 'customMenuVisible': True, 'shimPid': 42, 'shimLifecycle': {'events': [
                     {'kind': 'kernel-exit-observed', 'pid': 42}]}, 'results': [
                     {'type': 'contextmenu', 'constructor': 'PointerEvent', 'pointerType': 'mouse',
                      'button': 2, 'isTrusted': True} for _ in range(20)]}
        self.assertEqual(context_result(value)['trustedContextMenus'], 20)
        for mutate in (lambda v: v.update(status='partial'),
                       lambda v: v.update(attemptedIterations=0),
                       lambda v: v['results'][0].update(isTrusted=False),
                       lambda v: v['shimLifecycle'].update(events=[])):
            bad = copy.deepcopy(value)
            mutate(bad)
            with self.assertRaises(Failure):
                context_result(bad)

    def test_failure_projection_keeps_cause_but_drops_private_values(self):
        value = {'profile': 'PRIVATE', 'beforeCleanup': {'shim': {'events': [
            {'type': 'disconnected', 'reason': 'process-exited', 'causeReason': 'hello-timeout',
             'pid': 42, 'launchToken': 'PRIVATE', 'cookie': 'PRIVATE', 'causeTimeMs': 123}]}}}
        result = failure_snapshot(value)
        self.assertNotIn('PRIVATE', json.dumps(result))
        self.assertEqual(result['events'][0]['causeReason'], 'hello-timeout')

    def thin_macho(self, name, code=b'CODE', signature=b'signature'):
        command_size = 72 + 80 + 24
        offset = 32 + command_size
        header = struct.pack('<IIIIIIII', 0xFEEDFACF, 0x01000007, 3, 6, 2, command_size, 0, 0)
        segment = struct.pack('<II16sQQQQiiII', 0x19, 152, b'__TEXT', 0, 4096, 0, offset + len(code), 7, 5, 1, 0)
        section = struct.pack('<16s16sQQIIIIIIII', b'__text', b'__TEXT', 4096, len(code), offset, 2, 0, 0, 0x80000400, 0, 0, 0)
        uuid = struct.pack('<II', 0x1B, 24) + b'U' * 16
        path = self.root / name
        path.write_bytes(header + segment + section + uuid + code + signature)
        return path

    def test_signing_blob_difference_keeps_code_binding(self):
        before = macho(self.thin_macho('before'), 6)
        after = macho(self.thin_macho('after', signature=b'changed'), 6)
        changed = macho(self.thin_macho('changed', code=b'EVIL'), 6)
        self.assertNotEqual(before['sha256'], after['sha256'])
        self.assertEqual(before['sectionsSHA256'], after['sectionsSHA256'])
        self.assertNotEqual(before['sectionsSHA256'], changed['sectionsSHA256'])
        self.assertEqual(before['uuid'], after['uuid'])

    def test_wrong_architecture_or_type_cannot_bind(self):
        path = self.thin_macho('wrong')
        with self.assertRaises(Failure):
            macho(path, 1)
        data = bytearray(path.read_bytes())
        struct.pack_into('<I', data, 4, 0x0100000C)
        path.write_bytes(data)
        with self.assertRaises(Failure):
            macho(path, 6)


class DarwinCounterGuards(unittest.TestCase):
    def test_native_census_cannot_pass_when_system_ps_was_never_sampled(self):
        observations = {'samples': 4, 'completeSamples': 4, 'systemPsObserved': 0}
        with self.assertRaisesRegex(Failure, 'census-system-ps-not-observed'):
            monitor_native_preflight.require_observation(observations, 'darwin')
        monitor_native_preflight.require_observation(observations, 'linux')
        observations['systemPsObserved'] = 1
        monitor_native_preflight.require_observation(observations, 'darwin')
        observations['completeSamples'] = 0
        with self.assertRaisesRegex(Failure, 'census-insufficient-complete-samples'):
            monitor_native_preflight.require_observation(observations, 'darwin')

    def test_native_census_retains_denied_ps_from_retried_partial_samples(self):
        observations = dict.fromkeys(('samples', 'completeSamples', 'systemPsObserved',
                                     'differentOwnerObserved', 'psTaskDeniedSamples'), 0)
        activity = {'sampleComplete': True, 'inventorySummary': {'roles': {'other': 1}},
                    'partialSamples': [{'inventorySummary': {'roles': {'system-ps': 1}, 'differentOwner': 1},
                                        'formatExamples': [{'kind': 'permission',
                                                            'inventory': {'role': 'system-ps'}}]}]}
        monitor_native_preflight.record_observation(activity, observations)
        self.assertEqual(observations, {'samples': 1, 'completeSamples': 1, 'systemPsObserved': 1,
                                        'differentOwnerObserved': 1, 'psTaskDeniedSamples': 1})

    def test_native_census_counts_three_denied_attempts_without_counting_last_twice(self):
        observations = dict.fromkeys(('samples', 'completeSamples', 'systemPsObserved',
                                     'differentOwnerObserved', 'psTaskDeniedSamples'), 0)
        denied = {'inventorySummary': {'roles': {'system-ps': 1}, 'differentOwner': 1},
                  'formatExamples': [{'kind': 'permission', 'inventory': {'role': 'system-ps'}}]}
        activity = {**denied, 'sampleComplete': False, 'partialSamples': [denied, denied, denied]}
        monitor_native_preflight.record_observation(activity, observations)
        self.assertEqual(observations, {'samples': 1, 'completeSamples': 0, 'systemPsObserved': 3,
                                        'differentOwnerObserved': 3, 'psTaskDeniedSamples': 3})

    def sensor(self, rss=8 * 1024**2, user=2000000000, system_ticks=2000000000):
        library, system = mock.Mock(), mock.Mock()
        def timebase(pointer):
            pointer._obj.numer, pointer._obj.denom = 3, 2
            return 0
        system.mach_timebase_info.side_effect = timebase
        def query(pid, flavor, arg, pointer, size):
            self.assertEqual(arg, 1 if flavor == 3 else 0)
            value = pointer._obj
            bsd = value.bsd if flavor == 2 else value
            bsd.pid, bsd.uid, bsd.pgid = pid, os.geteuid(), 900
            bsd.start_sec, bsd.start_usec, bsd.status = 100, 7, 2
            if flavor == 2:
                task = value.task
                task.virtual_size, task.resident_size = 4 * 1024**3, rss
                task.total_user, task.total_system = user, system_ticks
                task.policy, task.threadnum, task.numrunning = 1, 3, 1
            return size
        library.proc_pidinfo.side_effect = query
        return DarwinActivity(library, system), library

    def test_native_task_layout_and_mach_timebase_conversion(self):
        sensor, _ = self.sensor()
        self.assertEqual((ctypes.sizeof(BSDInfo), ctypes.sizeof(TaskInfo), ctypes.sizeof(TaskAllInfo)),
                         (136, 96, 232))
        value = sensor.read(42, 900)
        self.assertEqual(value['status'], 'measured')
        self.assertEqual(value['rssMiB'], 8)
        self.assertEqual(value['cpuSeconds'], 6)
        self.assertEqual(value['state'], 'R')
        self.assertEqual(sensor.read(42, 900)['identityTag'], value['identityTag'])

    def test_measured_zero_differs_from_missing_task_info(self):
        sensor, library = self.sensor(rss=0)
        self.assertEqual(sensor.read(42, 900)['rssMiB'], 0)
        library.proc_pidinfo.side_effect = lambda *args: 0
        with mock.patch('darwin_activity.os.kill'):
            missing = sensor.read(42, 900)
        self.assertEqual(missing['status'], 'unavailable')
        self.assertNotIn('rssMiB', missing)

    def test_permission_or_zero_filled_records_cannot_prove_safe_memory(self):
        sensor, library = self.sensor()
        def denied(*args):
            ctypes.set_errno(errno.EPERM)
            return 0
        library.proc_pidinfo.side_effect = denied
        with mock.patch('darwin_activity.os.kill') as signal:
            self.assertEqual(sensor.read(42, 900)['reason'], 'permission')
        signal.assert_not_called()
        self.assertEqual(library.proc_pidinfo.call_count, 1)
        library.proc_pidinfo.side_effect = lambda pid, flavor, arg, pointer, size: size
        self.assertEqual(sensor.read(42, 900)['status'], 'unavailable')

    def test_short_bsd_info_is_not_disappearance_without_independent_proof(self):
        sensor, library = self.sensor()
        library.proc_pidinfo.side_effect = lambda *args: 0
        with mock.patch('darwin_activity.os.kill'):
            self.assertEqual(sensor.bsd(42)[0], 'unavailable')
        with mock.patch('darwin_activity.os.kill', side_effect=ProcessLookupError):
            status, _, reason = sensor.bsd(42)
            self.assertEqual((status, reason), ('gone', 'signal-zero-esrch'))

    def test_confirmed_zombie_or_replaced_generation_is_distinct_from_unknown(self):
        sensor, library = self.sensor()
        original = library.proc_pidinfo.side_effect
        def zombie(pid, flavor, arg, pointer, size):
            if flavor == 2:
                return 0
            original(pid, flavor, arg, pointer, size)
            pointer._obj.status = 5
            pointer._obj.pgid = 0
            return size
        library.proc_pidinfo.side_effect = zombie
        self.assertEqual(sensor.read(42, 900)['reason'], 'bsd-zombie')
        expected = {'pid': 42, 'startSec': 100, 'startUsec': 7}
        self.assertEqual(sensor.read(42, 900, expected)['reason'], 'bsd-zombie')
        expected['startSec'] = 99
        self.assertEqual(sensor.read(42, 900, expected)['reason'], 'generation-replaced')
        def invalid_state(*args):
            size = original(*args)
            args[3]._obj.bsd.status = 0
            return size
        library.proc_pidinfo.side_effect = invalid_state
        self.assertEqual(sensor.read(42, 900, expected)['status'], 'unavailable')
        library.proc_pidinfo.side_effect = original
        expected = {'pid': 42, 'startSec': 99, 'startUsec': 7}
        self.assertEqual(sensor.read(42, 900, expected)['reason'], 'generation-replaced')

    def test_only_bsd_query_enables_zombie_search_and_resets_errno(self):
        sensor, library = self.sensor()
        ctypes.set_errno(errno.EACCES)
        self.assertEqual(sensor.bsd(42)[0], 'measured')
        self.assertEqual(ctypes.get_errno(), 0)
        self.assertEqual(library.proc_pidinfo.call_args.args[1:3], (3, 1))
        sensor.read(42, 900)
        self.assertEqual(library.proc_pidinfo.call_args.args[1:3], (2, 0))
        with mock.patch('darwin_activity.os.kill') as kill:
            for pid in (0, -1):
                with self.assertRaisesRegex(ValueError, 'positive-process-id-required'):
                    sensor.bsd(pid)
            kill.assert_not_called()

    def test_full_task_zombie_with_exit_pgid_is_gone_after_generation_check(self):
        sensor, library = self.sensor()
        original = library.proc_pidinfo.side_effect
        def zombie(*args):
            size = original(*args)
            args[3]._obj.bsd.status, args[3]._obj.bsd.pgid = 5, 0
            return size
        library.proc_pidinfo.side_effect = zombie
        expected = {'pid': 42, 'startSec': 100, 'startUsec': 7}
        self.assertEqual(sensor.read(42, 900, expected)['reason'], 'bsd-zombie')
        expected['startSec'] = 99
        self.assertEqual(sensor.read(42, 900, expected)['reason'], 'generation-replaced')
        library.proc_pidinfo.side_effect = original
        self.assertEqual(sensor.read(42, 901)['reason'], 'membership-changed')

    def test_full_bsd_response_requires_valid_identity_birth_and_state(self):
        for field, value in [('pid', 43), ('start_sec', 0), ('start_usec', 1000000), ('status', 0), ('status', 6)]:
            with self.subTest(field=field, value=value):
                sensor, library = self.sensor()
                original = library.proc_pidinfo.side_effect
                def invalid(*args):
                    size = original(*args)
                    setattr(args[3]._obj, field, value)
                    return size
                library.proc_pidinfo.side_effect = invalid
                with mock.patch('darwin_activity.os.kill') as kill:
                    self.assertEqual(sensor.bsd(42)[0], 'unavailable')
                    kill.assert_not_called()

    def test_short_or_oversized_zombie_response_cannot_prove_gone(self):
        for returned in (0, 135, 137):
            with self.subTest(returned=returned):
                sensor, library = self.sensor()
                original = library.proc_pidinfo.side_effect
                def invalid(*args):
                    original(*args)
                    args[3]._obj.status = 5
                    return returned
                library.proc_pidinfo.side_effect = invalid
                with mock.patch('darwin_activity.os.kill'):
                    self.assertEqual(sensor.bsd(42)[0], 'unavailable')

    def test_failed_query_keeps_numeric_details_without_private_identity(self):
        sensor, library = self.sensor()
        library.proc_pidinfo.side_effect = lambda *args: 0
        with mock.patch('darwin_activity.os.kill'):
            value = sensor.group(f'900 42 10 {os.geteuid()} {os.getuid()} /private/hidden\n', 900)
        example = value['formatExamples'][0]
        self.assertEqual(example['taskQuery'], {'bytes': 0, 'expectedBytes': 232, 'errno': 0})
        self.assertEqual(example['bsdQuery'], {'bytes': 0, 'expectedBytes': 136, 'errno': 0})
        self.assertEqual(example['fallbackReason'], 'short-bsd-info')
        self.assertIsNone(value['rssMiB'])
        self.assertNotIn('start_sec', json.dumps(example))

    @unittest.skipUnless(sys.platform == 'darwin', 'requires real macOS libproc and an unreaped child')
    def test_real_mac_unreaped_child_is_gone_while_signal_zero_succeeds(self):
        sensor = DarwinActivity()
        read_fd, write_fd = os.pipe()
        pid = os.fork()
        if pid == 0:
            os.close(write_fd)
            os.read(read_fd, 1)
            os._exit(0)
        os.close(read_fd)
        try:
            state, birth, _ = sensor.bsd(pid)
            self.assertEqual(state, 'measured')
            expected = {'pid': pid, 'startSec': birth.start_sec, 'startUsec': birth.start_usec}
            os.close(write_fd)
            write_fd = None
            deadline = time.monotonic() + 5
            details = {}
            while time.monotonic() < deadline:
                state, value, reason = sensor.bsd(pid, details)
                if state == 'gone':
                    break
                time.sleep(0.01)
            self.assertEqual((state, reason), ('gone', 'bsd-zombie'))
            self.assertEqual(details['bytes'], 136)
            self.assertEqual((value.pid, value.start_sec, value.start_usec),
                             (pid, expected['startSec'], expected['startUsec']))
            self.assertEqual(value.status, 5)
            os.kill(pid, 0)
            self.assertEqual(sensor.read(pid, os.getpgrp(), expected)['reason'], 'bsd-zombie')
            expected['startSec'] -= 1
            self.assertEqual(sensor.read(pid, expected=expected)['reason'], 'generation-replaced')
        finally:
            if write_fd is not None:
                os.close(write_fd)
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            os.waitpid(pid, 0)

    def test_group_partial_totals_remain_unknown_and_tags_do_not_publish_names(self):
        sensor, _ = self.sensor()
        measured = sensor.read(42, 900)
        unknown = {'status': 'unavailable', 'reason': 'short-task-info', 'pidTag': sensor.tag(43)}
        with mock.patch.object(sensor, 'read', side_effect=[measured, unknown]):
            value = sensor.group(f'900 42 10 {os.geteuid()} {os.getuid()} /python\n'
                                 '900 43 42 0 501 /bin/ps\n901 99 1 0 0 /other\n', 900)
        self.assertFalse(value['sampleComplete'])
        self.assertIsNone(value['rssMiB'])
        self.assertIsNone(value['cpuSeconds'])
        self.assertEqual(value['rssLowerBoundMiB'], 8)
        self.assertEqual(value['unknownKinds'], {'short-task-info': 1})
        self.assertEqual(value['statesSource'], 'libproc-runnable-summary')
        self.assertEqual(value['formatExamples'][-1]['pidTag'], unknown['pidTag'])

    def test_darwin_runner_inventory_does_not_depend_on_ps_thread_state(self):
        sensor, _ = self.sensor()
        with mock.patch('support.sys.platform', 'darwin'), \
                mock.patch('darwin_activity.probe', return_value=sensor), \
                mock.patch('support.subprocess.run', return_value=mock.Mock(
                    stdout=f'900 42 10 {os.geteuid()} {os.getuid()} /python\n')) as ps:
            value = Runner.group_activity(900)
        self.assertEqual(ps.call_args.args[0][-1], 'pgid=,pid=,ppid=,uid=,ruid=,comm=')
        self.assertTrue(value['sampleComplete'])
        self.assertEqual(value['rssMiB'], 8)
        self.assertEqual(value['counterSource'], 'libproc-taskallinfo')

    def test_denied_inventory_metadata_is_diagnostic_only_and_never_excluded(self):
        sensor, library = self.sensor()
        def denied(*args):
            ctypes.set_errno(errno.EPERM)
            return 0
        library.proc_pidinfo.side_effect = denied
        with mock.patch('darwin_activity.os.geteuid', return_value=501), \
                mock.patch('darwin_activity.os.getuid', return_value=501):
            value = sensor.group('900 42 10 0 501 /bin/ps\n901 99 1 0 0 /secret\n', 900)
        self.assertEqual(value['unknownKinds'], {'permission': 1})
        self.assertFalse(value['sampleComplete'])
        self.assertIsNone(value['rssMiB'])
        self.assertIsNone(value['cpuSeconds'])
        self.assertEqual(value['confirmedGone'], 0)
        inventory = value['formatExamples'][0]['inventory']
        self.assertEqual(inventory['ownerRelation'], 'different')
        self.assertEqual(inventory['realOwnerRelation'], 'same')
        self.assertEqual(inventory['role'], 'system-ps')
        self.assertFalse(inventory['identityBound'])
        self.assertEqual(value['inventorySummary'], {
            'identityBound': False, 'sameOwner': 0, 'differentOwner': 1, 'metadataUnavailable': 0,
            'roles': {'system-ps': 1}})
        self.assertNotIn('/secret', json.dumps(value))
        self.assertNotIn('/bin/ps', json.dumps(value))
        self.assertEqual(library.proc_pidinfo.call_count, 1)

    def test_inventory_metadata_cannot_supply_generation_or_hide_malformed_member(self):
        sensor, library = self.sensor()
        def denied(*args):
            ctypes.set_errno(errno.EPERM)
            return 0
        library.proc_pidinfo.side_effect = denied
        inventory = (f'900 42 10 {os.geteuid()} {os.getuid()} /private/secret profile/host\n'
                     '900 43x 42 ? 501 /bin/ps\n900 ?\n')
        value = sensor.group(inventory, 900)
        self.assertEqual(value['unknownKinds'], {'permission': 1, 'inventory-format': 2})
        self.assertIsNone(value['formatExamples'][0]['identityTag'])
        self.assertEqual(value['formatExamples'][0]['inventory']['role'], 'other')
        self.assertNotIn('secret', json.dumps(value))
        self.assertNotIn('host', json.dumps(value))
        self.assertFalse(value['sampleComplete'])

    def test_missing_diagnostic_metadata_cannot_change_task_measurement_decision(self):
        sensor, _ = self.sensor()
        value = sensor.group('900 42 10 ? ?\n', 900)
        self.assertTrue(value['sampleComplete'])
        self.assertEqual(value['rssMiB'], 8)
        self.assertEqual(value['inventorySummary']['metadataUnavailable'], 1)

    def test_native_registry_cannot_hide_unknown_birth_or_memory(self):
        system = MacProcesses.__new__(MacProcesses)
        system.activity = mock.Mock()
        system.activity.bsd.return_value = ('unavailable', None, 'short-bsd-info')
        with self.assertRaisesRegex(Failure, 'native-birth-unavailable'):
            system.info(42)
        system.activity.read.return_value = {'status': 'unavailable'}
        with self.assertRaisesRegex(Failure, 'native-rss-unavailable'):
            system.rss(42, {'pid': 42})
        system.activity.bsd.return_value = ('gone', None, 'signal-zero-esrch')
        system.activity.read.return_value = {'status': 'gone', 'reason': 'signal-zero-esrch'}
        self.assertIsNone(system.info(42))
        self.assertEqual(system.rss(42, {'pid': 42}), 0)

    def test_resource_reserve_is_checked_even_when_monitor_would_be_unavailable(self):
        with tempfile.TemporaryDirectory() as temporary:
            limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
            runner = Runner(temporary, time.time(), limits)
            with mock.patch('support.shutil.disk_usage', return_value=mock.Mock(free=1024**3)), \
                    mock.patch.object(runner, 'group_activity') as sample:
                with self.assertRaisesRegex(Failure, 'free-disk-reserve'):
                    runner.run('low-disk', [sys.executable, '-c', 'import time; time.sleep(3)'],
                               temporary, os.environ.copy(), 360)
            sample.assert_not_called()
            self.assertEqual(runner.failure['code'], 'free-disk-reserve')
            self.assertFalse(runner.groups)


class SDKGuards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.plan = json.loads(Path(__file__).with_name('plan.json').read_text())
        pin = self.plan['sdk']
        tar_path = self.root / pin['entry']
        settings = plistlib.dumps({'Version': pin['version']})
        with tarfile.open(tar_path, 'w:xz') as archive:
            item = tarfile.TarInfo('MacOSX.sdk/SDKSettings.plist')
            item.size = len(settings)
            archive.addfile(item, io.BytesIO(settings))
        zip_path = self.root / 'input.zip'
        with zipfile.ZipFile(zip_path, 'w') as archive:
            archive.write(tar_path, arcname=pin['entry'])
        self.zip_bytes = zip_path.read_bytes()
        pin.update(zipBytes=len(self.zip_bytes), zipSHA256=digest(zip_path),
                   tarBytes=tar_path.stat().st_size, tarSHA256=digest(tar_path),
                   draftReaderTreeSHA256=scan_tar(tar_path)['canonicalSHA256'])
        self.plan_path = self.root / 'plan.json'
        self.plan_path.write_text(json.dumps(self.plan))
        self.now = datetime.datetime(2026, 10, 17, 16, tzinfo=datetime.timezone.utc)
        self.artifact = {'id': pin['artifact'], 'name': pin['name'], 'size_in_bytes': pin['zipBytes'],
                         'digest': 'sha256:' + pin['zipSHA256'], 'expired': False,
                         'workflow_run': {'id': pin['run']},
                         'expires_at': (self.now + datetime.timedelta(hours=1)).isoformat()}
        self.run = {'run_attempt': pin['attempt'], 'head_sha': self.plan['runtime']['ref'], 'status': 'completed'}
        self.api_opener, self.download_opener = mock.Mock(), mock.Mock()
        self.api_opener.open.side_effect = self.open_api
        self.download_opener.open.side_effect = lambda *_args, **_kwargs: io.BytesIO(self.zip_bytes)

    def open_api(self, request, **_kwargs):
        self.assertTrue(request.full_url.startswith('https://api.github.com/repos/Floorp-Projects/Floorp-Runtime/actions/'))
        if request.full_url.endswith('/zip'):
            raise urllib.error.HTTPError(request.full_url, 302, 'redirect', {'Location': 'https://example.invalid/fixed.zip'}, None)
        data = self.run if '/runs/' in request.full_url else self.artifact
        return io.BytesIO(json.dumps(data).encode())

    def fetch(self):
        fixed_now = self.now
        class Clock(datetime.datetime):
            @classmethod
            def now(cls, _tz=None):
                return fixed_now
        with mock.patch.dict(os.environ, {'SDK_READ_TOKEN': 'test-only-placeholder'}), \
                mock.patch.object(sys, 'argv', ['fetch_sdk.py', '--plan', str(self.plan_path),
                                               '--output', str(self.root / 'sdk-download')]), \
                mock.patch('fetch_sdk.datetime.datetime', Clock), \
                mock.patch('fetch_sdk.urllib.request.build_opener', side_effect=[self.api_opener, self.download_opener]):
            return fetch_sdk.main()

    def test_unexpired_sdk_with_less_than_six_hours_can_be_acquired(self):
        self.assertEqual(self.plan['limits']['jobMinutes'], 360)
        self.assertEqual(self.fetch(), 0)
        self.assertEqual(self.api_opener.open.call_count, 3)
        self.assertEqual(self.download_opener.open.call_count, 1)
        receipt = json.loads((self.root / 'sdk-download/receipt.json').read_text())
        self.assertEqual(receipt['tarSHA256'], self.plan['sdk']['tarSHA256'])
        self.assertFalse(receipt['signedURLPublished'])

    def test_expired_flag_or_elapsed_expiry_stops_before_download(self):
        for flagged, expiry in ((True, self.now + datetime.timedelta(hours=1)),
                                (False, self.now), (False, self.now - datetime.timedelta(seconds=1))):
            with self.subTest(flagged=flagged, expiry=expiry):
                self.artifact.update(expired=flagged, expires_at=expiry.isoformat())
                self.api_opener.open.reset_mock()
                with self.assertRaisesRegex(Failure, 'sdk-artifact-provenance' if flagged else 'sdk-expired'):
                    self.fetch()
                self.assertEqual(self.api_opener.open.call_count, 2)
                self.download_opener.open.assert_not_called()
                self.assertFalse((self.root / 'sdk-download').exists())

    def test_http_error_never_uses_a_bypass_download(self):
        self.api_opener.open.side_effect = urllib.error.HTTPError('https://api.github.com/', 410, 'gone', {}, None)
        with self.assertRaises(urllib.error.HTTPError):
            self.fetch()
        self.download_opener.open.assert_not_called()

    def test_download_deadline_and_fixed_zip_digest_remain_enforced(self):
        with mock.patch('support.time.monotonic', side_effect=[0, 601]):
            with self.assertRaisesRegex(Failure, 'download-deadline'):
                self.fetch()
        self.assertFalse((self.root / 'sdk-download/receipt.json').exists())
        (self.root / 'sdk-download/sdk.zip').unlink()
        (self.root / 'sdk-download').rmdir()
        self.download_opener.open.side_effect = lambda *_args, **_kwargs: io.BytesIO(b'x' * len(self.zip_bytes))
        with self.assertRaisesRegex(Failure, 'download-hash-mismatch'):
            self.fetch()

    def test_local_verified_sdk_materializes_without_service_expiry_queries(self):
        self.fetch()
        self.artifact.update(expired=True, expires_at=(self.now - datetime.timedelta(hours=1)).isoformat())
        with mock.patch('materialize.__file__', str(self.root / 'materialize.py')), \
                mock.patch.object(sys, 'argv', ['materialize.py', 'sdk', '--task', str(self.root)]), \
                mock.patch('urllib.request.build_opener', side_effect=AssertionError('unexpected network')):
            self.assertEqual(materialize.main(), 0)
        proof = json.loads((self.root / 'sdk-materialized.json').read_text())
        self.assertEqual(proof['canonicalSHA256'], self.plan['sdk']['draftReaderTreeSHA256'])
        self.assertTrue((Path(proof['path']) / 'SDKSettings.plist').is_file())


if __name__ == '__main__':
    unittest.main()

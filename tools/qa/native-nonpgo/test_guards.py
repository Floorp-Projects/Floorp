# SPDX-License-Identifier: MPL-2.0
import copy
import contextlib
import errno
import io
import json
import os
from pathlib import Path
import plistlib
import struct
import subprocess
import sys
import tarfile
import tempfile
import textwrap
import time
import unittest
from unittest import mock
import native_cause
from support import Failure, ProgressWatch, Runner, clean_environment, extract_tar, scan_tar
from native_suite import cause_failure_snapshot, context_result, failure_snapshot
from pipeline import macho, prepare_harness, verify_config
from owned_native import OwnedNative


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
        with mock.patch('support.subprocess.run', return_value=mock.Mock(stdout=output)):
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
        with mock.patch('support.subprocess.run', side_effect=[mock.Mock(stdout=unknown), mock.Mock(stdout=complete)]), \
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
        with mock.patch('support.subprocess.run', return_value=mock.Mock(stdout='900 901 0 0:00.00 ?\n')) as command, \
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
        with mock.patch.dict(os.environ, {'GITHUB_WORKSPACE': str(workspace), 'TASK_ROOT': str(self.root)}), \
                mock.patch.object(Runner, 'run', return_value=None), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(Failure, 'monitor-preflight-no-complete-sample'):
                exec(compile(script, 'workflow-monitor-preflight', 'exec'), {})
        proof = json.loads((self.root / 'monitor-preflight.json').read_text())
        self.assertEqual(proof['status'], 'failed')
        self.assertEqual(proof['firstFailure']['code'], 'monitor-preflight-no-complete-sample')

    def test_unavailable_monitor_preserves_failure_and_reclaims_owned_group(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        runner = Runner(self.root, time.time(), limits)
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
        self.assertEqual(runner.monitor_diagnostics[0]['unknownKinds'], {'state-unavailable': 1})

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


if __name__ == '__main__':
    unittest.main()

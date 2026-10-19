# SPDX-License-Identifier: MPL-2.0
import contextlib
import errno
import importlib.util
import io
import json
import os
from pathlib import Path
import plistlib
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


HERE = Path(__file__).resolve().parent


def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, HERE/filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


d = load('failure_diagnostics_under_test', 'shim_failure_diagnostics.py')
context = load('context_with_failure_diagnostics', 'test-context-menu.py')
runtime = load('runtime_with_failure_diagnostics', 'test-runtime.py')


def host():
    process = mock.Mock(returncode=0)
    process.poll.return_value = 0
    value = SimpleNamespace(pid=987654321, identity=(987654321, 'PRIVATE-START', 'PRIVATE-ARGV'),
        verified=True, launch_services=True, exited=False, exit_code=None,
        requested_signal=None, process=process, poll=mock.Mock(return_value=None))
    return value


def history(count=30):
    rows = [{'type': 'connected' if n == 1 else 'disconnected', 'timeMs': 100+n,
        'sequence': n, 'payload': {'pid': 987654321, 'reason': 'process-exited',
            'causeReason': 'hello-timeout', 'causeTimeMs': 99, 'connectedBeforeDisconnect': False,
            'nonce': 'PRIVATE-NONCE', 'argv': 'PRIVATE-ARGV'}} for n in range(1, count+1)]
    return {'status': 'observed', 'count': count, 'events': rows[-16:],
        'firstEvent': rows[0], 'firstDisconnected': rows[1], 'malformedEvents': 0,
        'truncated': count > 16}


def client(events=None):
    value = mock.Mock()
    value.socket.gettimeout.return_value = 60
    value.script.return_value = events or history()
    return value


class ProjectionTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('node'), 'Node is needed to execute the browser observer control')
    def test_actual_observer_keeps_scope_first_cause_and_bounded_history(self):
        source = d.observer_script('globalThis.probe')
        script = '''globalThis.probe={};
globalThis.Services={obs:{addObserver(value){globalThis.observer=value;}}};
new Function(SOURCE)('wanted');
for(let n=1;n<=30;n++)observer.observe(null,null,JSON.stringify({
appId:'wanted',type:n===1?'connected':'disconnected',payload:{pid:987654321,
reason:'process-exited',causeReason:n===2?'hello-timeout':'PRIVATE-REASON',nonce:'PRIVATE-NONCE'}}));
observer.observe(null,null,JSON.stringify({appId:'PRIVATE-OTHER',type:'disconnected',payload:{}}));
observer.observe(null,null,'INVALID-PRIVATE-JSON');
console.log(JSON.stringify(probe.__failureDiagnostics));
'''.replace('SOURCE', json.dumps(source))
        completed = subprocess.run([shutil.which('node'), '-e', script], capture_output=True,
                                   check=True, timeout=5, text=True)
        value = json.loads(completed.stdout)
        self.assertEqual(value['count'], 30); self.assertEqual(len(value['events']), 16)
        self.assertEqual(value['firstEvent']['sequence'], 1)
        self.assertEqual(value['firstDisconnected']['sequence'], 2)
        self.assertEqual(value['firstDisconnected']['causeReason'], 'hello-timeout')
        self.assertEqual(value['malformedEvents'], 1)
        self.assertNotIn('PRIVATE', completed.stdout); self.assertNotIn('987654321', completed.stdout)

    def test_event_history_keeps_first_and_terminal_separately_from_recent_sixteen(self):
        result = d.event_history(history())
        self.assertEqual(len(result['events']), 16)
        self.assertEqual(result['events'][0]['sequence'], 15)
        self.assertEqual(result['firstEvent']['sequence'], 1)
        self.assertEqual(result['firstDisconnected']['sequence'], 2)
        self.assertTrue(result['truncated'])
        serialized = json.dumps(result)
        self.assertNotIn('PRIVATE', serialized); self.assertNotIn('987654321', serialized)

    def test_unknown_values_and_boolean_numbers_are_not_coerced(self):
        row = d.event({'type': 'disconnected', 'sequence': True,
            'payload': {'pid': True, 'reason': {'PRIVATE': 'DATA'}, 'causeReason': 'PRIVATE-CAUSE',
                        'causeTimeMs': 'PRIVATE-TIME', 'connectedBeforeDisconnect': 'true'}})
        self.assertIsNone(row['sequence']); self.assertIsNone(row['reason'])
        self.assertEqual(row['causeReason'], 'other'); self.assertIsNone(row['causeTimeMs'])
        self.assertIsNone(row['connectedBeforeDisconnect'])
        self.assertNotIn('PRIVATE', json.dumps(row))
        self.assertEqual(d.event_history(None)['status'], 'unavailable')
        self.assertIsNone(d.event_history(None)['count'])

    def test_monitor_projection_drops_pids_private_text_and_unrecognized_fields(self):
        rows = [{'kind': 'kernel-exit-observed', 'pid': 987654321, 'exitCode': 1,
                 'signal': None, 'exitStatusAvailable': True, 'errorType': 'PRIVATE',
                 'argv': 'PRIVATE', 'monotonicNs': n} for n in range(30)]
        value = d.monitor_projection({'status': 'exit-observed', 'pid': 987654321, 'events': rows})
        self.assertEqual(len(value['events']), 16)
        self.assertTrue(value['identityObserved']); self.assertFalse(value['kernelTimestampAvailable'])
        self.assertIsNone(value['nativeAuthenticationCompleted'])
        self.assertNotIn('PRIVATE', json.dumps(value)); self.assertNotIn('987654321', json.dumps(value))
        self.assertIsNone(d.monitor_projection({'status': 'not-observed', 'pid': None})['identityObserved'])

    def test_public_reprojection_handles_malformed_and_private_injected_fields(self):
        raw = {'schema': 1, 'PRIVATE': '/PRIVATE', 'firstFunctionalFailure': {
            'phase': {'PRIVATE': 'x'}, 'kind': 'PRIVATE', 'code': ['PRIVATE'], 'error': 'PRIVATE'},
            'firstReportedNativeCause': {'type': 'disconnected', 'payload': {'reason': 'PRIVATE'}},
            'cleanupOperations': [{'operation': {'PRIVATE': 'x'}}],
            'beforeCleanup': {'host': {'status': ['PRIVATE'], 'argv': '/PRIVATE'},
                'childMonitor': {'status': {'PRIVATE': 'x'}, 'events': [{'kind': {'PRIVATE': 'x'}}]}},
            'diagnosticErrors': [{'operation': ['PRIVATE'], 'kind': 'RuntimeError', 'text': 'PRIVATE'}]}
        value = d.project_diagnostics(raw)
        self.assertTrue(value['available']); self.assertNotIn('PRIVATE', json.dumps(value))
        self.assertEqual(d.project_diagnostics(None), {'schema': 1, 'available': False})


class HostTests(unittest.TestCase):
    def test_launcher_success_is_not_browser_death_before_adoption(self):
        value = host(); value.pid = value.identity = None; value.verified = False
        value.poll.return_value = 0
        result = d.host_snapshot(value)
        self.assertEqual(result['launcherExitCode'], 0)
        self.assertEqual(result['browserLiveness'], 'not-observed')
        self.assertIsNone(result['browserExitCode']); self.assertFalse(result['browserExitStatusAvailable'])

    def test_exit_liveness_sentinel_is_not_a_successful_browser_exit_code(self):
        value = host(); value.exited = True; value.poll.return_value = 0
        result = d.host_snapshot(value)
        self.assertEqual(result['browserLiveness'], 'exit-observed')
        self.assertIsNone(result['browserExitCode'])
        value.exit_code = -signal.SIGSEGV
        result = d.host_snapshot(value)
        self.assertEqual(result['browserSignal'], signal.SIGSEGV)
        self.assertEqual(result['browserExitCode'], -signal.SIGSEGV)
        self.assertEqual(result['launcherExitCode'], 0)

    def test_host_probe_failure_keeps_launcher_and_unknown_liveness(self):
        value = host(); value.poll.side_effect = PermissionError(errno.EACCES, 'PRIVATE')
        result = d.host_snapshot(value)
        self.assertEqual(result['status'], 'unavailable')
        self.assertEqual(result['browserLiveness'], 'unavailable')
        self.assertEqual(result['failure']['errno'], errno.EACCES)
        self.assertEqual(result['launcherExitCode'], 0); self.assertNotIn('PRIVATE', json.dumps(result))


class PersistenceTests(unittest.TestCase):
    def test_precleanup_is_private_durable_and_unchanged_by_signal_or_cleanup_failure(self):
        with tempfile.TemporaryDirectory(prefix='PRIVATE-') as temporary:
            recorder = d.Recorder(temporary); browser = host()
            recorder.functional_failure('authenticate', RuntimeError('Timed out waiting for Shim authentication'))
            recorder.monitor = mock.Mock()
            recorder.monitor.snapshot.return_value = {'status': 'monitoring', 'pid': 987654321, 'events': []}
            peer = client()
            recorder.before_cleanup(browser, peer, 'window.testState')
            path = Path(temporary)/'native-before-cleanup.json'
            original = path.read_bytes()
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)
            self.assertNotIn('PRIVATE', original.decode()); self.assertNotIn('987654321', original.decode())
            def failing_close():
                self.assertEqual(path.read_bytes(), original)
                browser.requested_signal = signal.SIGTERM
                browser.exit_code, browser.exited = -signal.SIGTERM, True
                raise RuntimeError('PRIVATE-CLEANUP')
            self.assertFalse(recorder.cleanup('browser-host-close', failing_close))
            recorder.after_cleanup(browser)
            recorder.before_cleanup(browser, peer, 'window.testState')
            self.assertEqual(path.read_bytes(), original)
            result = recorder.summary()
            self.assertIsNone(result['beforeCleanup']['host']['requestedSignal'])
            self.assertEqual(result['afterCleanup']['host']['requestedSignal'], signal.SIGTERM)
            self.assertEqual(result['firstFunctionalFailure']['code'], 'shim-authentication-timeout')
            self.assertEqual(result['firstReportedNativeCause']['causeReason'], 'hello-timeout')
            self.assertEqual(result['firstCleanupFailure']['operation'], 'browser-host-close')
            self.assertNotIn('PRIVATE', json.dumps(result))
            self.assertEqual(peer.socket.settimeout.call_args_list, [mock.call(3), mock.call(60)])

    def test_cleanup_overflow_preserves_first_failure_and_keeps_latest_sixteen(self):
        with tempfile.TemporaryDirectory() as temporary:
            recorder = d.Recorder(temporary)
            recorder.functional_failure('authenticate', RuntimeError('Timed out waiting for Shim authentication'))
            def fail(): raise PermissionError(errno.EPERM, 'PRIVATE')
            recorder.cleanup('browser-service-stop', fail)
            for _ in range(25): recorder.cleanup('browser-log-close', lambda: None)
            result = recorder.summary()
            self.assertEqual(result['cleanupCount'], 26); self.assertEqual(len(result['cleanupOperations']), 16)
            self.assertEqual(result['firstCleanupFailure']['failure']['errno'], errno.EPERM)
            self.assertEqual(result['firstFunctionalFailure']['code'], 'shim-authentication-timeout')
            self.assertTrue(recorder.failed()); self.assertNotIn('PRIVATE', json.dumps(result))

    def test_rpc_and_child_snapshot_failures_are_unknown_and_do_not_block_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            recorder = d.Recorder(temporary); peer = client()
            peer.context.side_effect = RuntimeError('PRIVATE-RPC')
            recorder.monitor = mock.Mock(); recorder.monitor.snapshot.side_effect = PermissionError(errno.EACCES, 'PRIVATE')
            recorder.before_cleanup(host(), peer, 'window.testState')
            result = recorder.summary()
            self.assertEqual(result['beforeCleanup']['events']['status'], 'unavailable')
            self.assertIsNone(result['beforeCleanup']['events']['count'])
            self.assertEqual(result['beforeCleanup']['childMonitor']['status'], 'unavailable')
            self.assertIsNone(result['beforeCleanup']['childMonitor']['identityObserved'])
            self.assertTrue(result['diagnosticsIncomplete'])
            self.assertTrue((Path(temporary)/'native-before-cleanup.json').is_file())
            self.assertEqual(peer.socket.settimeout.call_args_list[-1], mock.call(60))
            self.assertNotIn('PRIVATE', json.dumps(result))

    def test_precleanup_write_failure_still_allows_post_cleanup_and_first_error(self):
        with tempfile.TemporaryDirectory() as temporary:
            recorder = d.Recorder(temporary)
            recorder.functional_failure('authenticate', RuntimeError('Timed out waiting for Shim authentication'))
            original_open = d.os.open
            def opening(path, *args):
                if Path(path).name == 'native-before-cleanup.json':
                    raise PermissionError(errno.EACCES, 'PRIVATE-PATH')
                return original_open(path, *args)
            with mock.patch.object(d.os, 'open', side_effect=opening):
                recorder.before_cleanup(host(), client(), 'window.testState')
                self.assertTrue(recorder.cleanup('browser-host-close', lambda: None))
                recorder.after_cleanup(host())
            self.assertTrue((Path(temporary)/'native-after-cleanup.json').exists())
            result = recorder.summary()
            self.assertTrue(result['diagnosticsIncomplete'])
            self.assertEqual(result['firstFunctionalFailure']['code'], 'shim-authentication-timeout')
            self.assertEqual(result['diagnosticErrors'][0]['operation'], 'write-native-before-cleanup.json')
            self.assertNotIn('PRIVATE', json.dumps(result))


class EntrypointTests(unittest.TestCase):
    def test_context_auth_timeout_survives_service_client_and_host_cleanup_errors(self):
        with tempfile.TemporaryDirectory(prefix='PRIVATE-') as temporary:
            root = Path(temporary); browser = root/'Test.app'
            (browser/'Contents/MacOS').mkdir(parents=True)
            (browser/'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': 'browser'}))
            output = root/'evidence'; bundle = root/'private-shim.app'
            browser_host = host(); browser_host.verify_session = mock.Mock()
            peer = client()
            def script(source, *_args):
                if 'hostIdentityJSON' in source: return '{}'
                if 'firstDisconnected:d.firstDisconnected' in source: return history()
                if 'return {crashes:' in source: return {'crashes': [], 'failures': []}
                if '.sendControl(' in source: raise RuntimeError('PRIVATE-SERVICE-CLEANUP')
                return True
            peer.script.side_effect = script
            peer.close.side_effect = RuntimeError('PRIVATE-CLIENT-CLEANUP')
            def close_host():
                paths = list(output.rglob('native-before-cleanup.json'))
                self.assertEqual(len(paths), 1)
                self.assertEqual(json.loads(paths[0].read_text())['firstFunctionalFailure']['code'], 'shim-authentication-timeout')
                browser_host.requested_signal = signal.SIGTERM
                raise RuntimeError('PRIVATE-HOST-CLEANUP')
            browser_host.close = mock.Mock(side_effect=close_host)
            def waiting(description, _callback, _timeout):
                if description == 'Marionette': return peer
                raise RuntimeError('Timed out waiting for Shim authentication')
            argv = ['test-context-menu.py', '--browser', str(browser), '--output', str(output)]
            with mock.patch.object(sys, 'argv', argv), mock.patch.object(context.h, 'run'), \
                 mock.patch.object(context.h, 'unused_port', return_value=12345), \
                 mock.patch.object(context.h, 'HostProcess', return_value=browser_host), \
                 mock.patch.object(context.h, 'seal_bundle', return_value=bundle), \
                 mock.patch.object(context.h, 'wait_for', side_effect=waiting), \
                 mock.patch.object(context.h.failure_diagnostics.Recorder, 'start_monitor'), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(context.main(), 1)
            paths = list(output.rglob('result.json')); self.assertEqual(len(paths), 1)
            report = json.loads(paths[0].read_text()); safe = report['failureDiagnostics']
            self.assertEqual(report['error'], 'Timed out waiting for Shim authentication')
            self.assertEqual(safe['firstFunctionalFailure']['code'], 'shim-authentication-timeout')
            self.assertEqual(safe['firstCleanupFailure']['operation'], 'browser-service-stop')
            self.assertEqual(len([row for row in safe['cleanupOperations'] if row['failure']]), 3)
            self.assertEqual(safe['afterCleanup']['host']['requestedSignal'], signal.SIGTERM)
            self.assertNotIn('PRIVATE', json.dumps(safe)); browser_host.close.assert_called_once()
            self.assertTrue(list(output.rglob('native-after-cleanup.json')))

    def test_runtime_early_failure_still_writes_both_safe_snapshots(self):
        with tempfile.TemporaryDirectory(prefix='PRIVATE-') as temporary:
            root = Path(temporary); browser = root/'Test.app'
            (browser/'Contents/MacOS').mkdir(parents=True)
            (browser/'Contents/MacOS/floorp-app-shim').touch()
            (browser/'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': 'browser'}))
            output = root/'evidence'
            argv = ['test-runtime.py', '--browser', str(browser), '--output', str(output)]
            with mock.patch.object(sys, 'argv', argv), mock.patch.object(runtime.sys, 'platform', 'darwin'), \
                 mock.patch.object(runtime, 'run', side_effect=RuntimeError('PRIVATE-SIGNATURE-DETAIL')), \
                 contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(runtime.main(), 1)
            report = json.loads(next(output.rglob('report.json')).read_text())
            safe = report['failureDiagnostics']
            self.assertEqual(safe['firstFunctionalFailure']['phase'], 'host-signature')
            self.assertEqual(safe['beforeCleanup']['host']['status'], 'not-started')
            self.assertTrue(list(output.rglob('native-before-cleanup.json')))
            self.assertTrue(list(output.rglob('native-after-cleanup.json')))
            self.assertNotIn('PRIVATE', json.dumps(safe))


if __name__ == '__main__':
    unittest.main()

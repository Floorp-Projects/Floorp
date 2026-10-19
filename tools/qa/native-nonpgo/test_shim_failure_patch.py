# SPDX-License-Identifier: MPL-2.0
"""Exercise recording hooks on the pinned, patched QA harness without a browser.

The real prepare_harness patch/hash checks and private file writes run here.
Browser IPC, kernel monitors and signing are mocked; this is no Mac GUI proof.
"""
import ast
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import plistlib
import py_compile
import signal
import stat
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest import mock

from monitor_native_preflight import verify_harness
from pipeline import prepare_harness
from support import digest


HERE = Path(__file__).resolve().parent
FIXTURE = HERE.parents[2]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def history():
    rows = [{'type': 'connected' if n == 1 else 'disconnected', 'sequence': n, 'timeMs': 100+n,
             'payload': {'pid': 987654321, 'reason': 'process-exited', 'causeReason': 'hello-timeout',
                         'causeTimeMs': 99, 'connectedBeforeDisconnect': False, 'private': 'PRIVATE'}}
            for n in range(1, 31)]
    return {'status': 'observed', 'count': 30, 'events': rows[-16:], 'firstEvent': rows[0],
            'firstDisconnected': rows[1], 'malformedEvents': 0, 'truncated': True}


class ShimFailurePatchTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.harness = self.root / 'harness'
        self.app = self.harness / 'tools/app-shim'
        self.plan = json.loads((HERE / 'plan.json').read_text())
        for pin in self.plan['harness']['files']:
            target = self.harness / pin['path']
            target.parent.mkdir(parents=True, exist_ok=True)
            fixed = HERE / 'fixtures' / ('harness-' + target.name)
            source = fixed if fixed.is_file() else FIXTURE / pin['path']
            self.assertEqual(digest(source), pin['sha256'])
            target.write_bytes(source.read_bytes())
        verify_harness(self.harness, self.plan)
        runner = mock.Mock()
        def execute(_stage, command, cwd, env, minutes):
            self.assertEqual(minutes, self.plan['limits']['jobMinutes'])
            subprocess.run(command, cwd=cwd, env=env, check=True, capture_output=True)
        runner.run.side_effect = execute
        self.proof = prepare_harness(self.plan, self.harness, FIXTURE, runner, os.environ.copy())
        verify_harness(self.harness, self.plan, prepared=True)
        # Keep plain imports in the fixed context script local to this test.
        modules = mock.patch.dict(sys.modules, {
            'shim_diagnostics': load('qa_patch_diagnostics', self.app / 'shim_diagnostics.py'),
            'shim_lifecycle': load('qa_patch_lifecycle', self.app / 'shim_lifecycle.py')})
        modules.start()
        self.addCleanup(modules.stop)
        self.runtime = load('qa_patch_runtime', self.app / 'test-runtime.py')
        self.context = load('qa_patch_context', self.app / 'test-context-menu.py')

    def test_patch_compiles_and_preserves_startup_ownership_class_and_context_observer(self):
        self.assertEqual(self.proof['failureRecording']['patchSHA256'],
                         self.plan['harness']['failureDiagnosticsPatch']['sha256'])
        for filename in ('test-runtime.py', 'test-context-menu.py', 'shim_failure_diagnostics.py',
                         'shim_lifecycle.py', 'shim_diagnostics.py', 'native_processes.py'):
            py_compile.compile(str(self.app / filename), str(self.root / (filename + '.pyc')), doraise=True)
        patched = (self.app / 'test-runtime.py').read_text()
        # Reverse only the recording patch to recover the already verified
        # startup implementation, then compare the whole ownership class AST.
        patch = FIXTURE / self.plan['harness']['failureDiagnosticsPatch']['path']
        subprocess.run(['git', 'apply', '--reverse', str(patch)], cwd=self.harness,
                       check=True, capture_output=True)
        self.assertEqual(digest(self.app / 'test-runtime.py'),
                         self.plan['harness']['startupPatch']['patchedSHA256'])
        def host_class(source):
            return next(node for node in ast.parse(source).body
                        if isinstance(node, ast.ClassDef) and node.name == 'HostProcess')
        self.assertEqual(ast.dump(host_class((self.app / 'test-runtime.py').read_text())),
                         ast.dump(host_class(patched)))
        # Check the prepared source captured before reversing, without changing
        # the functional arrays or starting a second child monitor.
        subprocess.run(['git', 'apply', str(patch)], cwd=self.harness, check=True, capture_output=True)
        verify_harness(self.harness, self.plan, prepared=True)
        text = (self.app / 'test-context-menu.py').read_text()
        for preserved in ("'attemptedIterations':0", "report['attemptedIterations'] += 1",
                          "report['beforeCleanup'] = snapshot()", "report['shimLifecycle'] = monitor.snapshot()",
                          'diagnostics.write_first_failure(output, report)',
                          "'crash':report['shimCrash']", 'diagnostics.save_streams(private_logs, output)',
                          'authEvents:[],authCount:0', 'if({STATE}.authEvents.length>64)'):
            self.assertIn(preserved, text)
        self.assertEqual(text.count('lifecycle.ShimMonitor('), 1)
        self.assertIn('failure_recorder.monitor = monitor', text)
        self.assertNotIn('failure_recorder.start_monitor(', text)

    def test_fixed_context_auth_timeout_keeps_qa_counts_and_precleanup_before_three_cleanup_errors(self):
        context = self.context
        browser = self.root / 'Test.app'
        (browser / 'Contents/MacOS').mkdir(parents=True)
        (browser / 'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': 'browser'}))
        output, bundle = self.root / 'evidence', self.root / 'private-shim.app'
        process = mock.Mock(returncode=0)
        process.poll.return_value = 0
        host = SimpleNamespace(pid=987654321, identity=(987654321, 'PRIVATE-START', 'PRIVATE-ARGV'),
            verified=True, launch_services=True, exited=False, exit_code=None, returncode=None,
            requested_signal=None, process=process, poll=mock.Mock(return_value=None), verify_session=mock.Mock())
        peer = mock.Mock()
        peer.socket.gettimeout.return_value = 60
        call_order = []
        def pre_is_persisted():
            paths = list(output.rglob('native-before-cleanup.json'))
            self.assertEqual(len(paths), 1)
            raw = json.loads(paths[0].read_text())
            self.assertEqual(raw['firstFunctionalFailure']['code'], 'shim-authentication-timeout')
            self.assertIsNone(raw['host']['requestedSignal'])
            self.assertEqual(stat.S_IMODE(paths[0].stat().st_mode), 0o600)
        def script(source, *_args):
            if 'hostIdentityJSON' in source:
                return '{}'
            if '__failureDiagnostics={' in source:
                call_order.append('recording-observer')
            if '.launchApp(' in source:
                call_order.append('launch')
            if 'firstDisconnected:d.firstDisconnected' in source:
                return history()
            if 'authCount:' in source and 'return {events:' in source:
                return {'events': history()['events'], 'authCount': 30, 'crashCount': 0, 'failureCount': 0}
            if '.sendControl(' in source:
                pre_is_persisted()
                raise RuntimeError('PRIVATE NS_ERROR_NOT_AVAILABLE')
            return True
        peer.script.side_effect = script
        peer.close.side_effect = RuntimeError('PRIVATE-CLIENT-CLEANUP')
        def close_host():
            pre_is_persisted()
            host.requested_signal = signal.SIGTERM
            host.exit_code = host.returncode = -signal.SIGTERM
            host.exited = True
            raise RuntimeError('PRIVATE-HOST-CLEANUP')
        host.close = mock.Mock(side_effect=close_host)
        monitor = mock.Mock(status='exit-observed', pid=987654321, identity=(987654321, 123, 4),
                            started_wall=time.time())
        monitor.snapshot.return_value = {'status': 'exit-observed', 'pid': 987654321, 'events': [
            {'kind': 'kernel-exit-observed', 'exitCode': 7, 'identityVerified': True, 'argv': 'PRIVATE'}]}
        def waiting(description, _callback, _timeout):
            if description == 'Marionette':
                return peer
            self.assertEqual(description, 'Shim authentication')
            raise RuntimeError('Timed out waiting for Shim authentication')
        argv = ['test-context-menu.py', '--browser', str(browser), '--output', str(output)]
        with mock.patch.object(sys, 'argv', argv), mock.patch.object(context.h, 'run'), \
             mock.patch.object(context.h, 'unused_port', return_value=12345), \
             mock.patch.object(context.h, 'HostProcess', return_value=host), \
             mock.patch.object(context.h, 'seal_bundle', return_value=bundle), \
             mock.patch.object(context.h, 'wait_for', side_effect=waiting), \
             mock.patch.object(context.lifecycle, 'binary_summary', return_value={'status': 'observed'}), \
             mock.patch.object(context.lifecycle, 'ShimMonitor', return_value=monitor) as make_monitor, \
             mock.patch.object(context.lifecycle, 'crash_summary', return_value={'status': 'not-observed'}), \
             mock.patch.object(context.lifecycle, 'system_log_summary', return_value={'status': 'observed'}), \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(context.main(), 1)
        report = json.loads(next(output.rglob('result.json')).read_text())
        safe = report['failureDiagnostics']
        first = json.loads(next(output.rglob('first-failure.json')).read_text())
        self.assertEqual(report['requestedIterations'], 20)
        self.assertEqual(report['attemptedIterations'], 0)
        self.assertEqual(report['phase'], 'authenticate-shim')
        self.assertEqual(first['error']['timeout'], 'Shim authentication')
        self.assertEqual(first['attemptedIterations'], 0)
        self.assertEqual(report['error']['timeout'], 'Shim authentication')
        self.assertEqual(report['beforeCleanup']['shim']['authEventCount'], 30)
        self.assertEqual(report['beforeCleanup']['crashCount'], 0)
        self.assertNotIn('stateError', report['beforeCleanup'])
        self.assertIn('streams', report)
        self.assertEqual(report['shimLifecycle']['status'], 'exit-observed')
        self.assertEqual(safe['firstFunctionalFailure']['phase'], 'authenticate-shim')
        self.assertEqual(safe['firstFunctionalFailure']['code'], 'shim-authentication-timeout')
        self.assertEqual(safe['firstReportedNativeCause']['causeReason'], 'hello-timeout')
        self.assertEqual(safe['firstReportedNativeCause']['sequence'], 2)
        self.assertEqual(safe['firstCleanupFailure']['operation'], 'browser-service-stop')
        self.assertEqual(len([row for row in safe['cleanupOperations'] if row['failure']]), 3)
        self.assertEqual(safe['afterCleanup']['host']['requestedSignal'], signal.SIGTERM)
        self.assertIsNone(safe['beforeCleanup']['host']['browserExitCode'])
        self.assertEqual(safe['beforeCleanup']['host']['launcherExitCode'], 0)
        self.assertEqual(safe['afterCleanup']['host']['browserExitCode'], -signal.SIGTERM)
        self.assertEqual([row['step'] for row in report['cleanupErrors']], ['shim', 'client', 'host'])
        self.assertEqual(report['cleanupErrors'][0]['codes'], ['NS_ERROR_NOT_AVAILABLE'])
        self.assertEqual(call_order, ['recording-observer', 'launch'])
        make_monitor.assert_called_once()
        monitor.start.assert_called_once()
        monitor.close.assert_called_once()
        self.assertTrue(list(output.rglob('before-cleanup.json')))
        self.assertTrue(list(output.rglob('native-after-cleanup.json')))
        self.assertNotIn('PRIVATE', json.dumps(safe))
        self.assertNotIn('987654321', json.dumps(safe))

    def test_fixed_runtime_early_failure_keeps_pre_post_and_original_functional_failure(self):
        runtime = self.runtime
        browser = self.root / 'Test.app'
        (browser / 'Contents/MacOS').mkdir(parents=True)
        (browser / 'Contents/MacOS/floorp-app-shim').touch()
        (browser / 'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': 'browser'}))
        output = self.root / 'evidence'
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

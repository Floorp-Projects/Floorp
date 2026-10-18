# SPDX-License-Identifier: MPL-2.0
import errno
import contextlib
import io
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from types import SimpleNamespace
from unittest import mock

from monitor_ps_lifetime import child_status
from support import Failure, Runner
from pipeline import validation_outcome


class OwnedCleanupGuards(unittest.TestCase):
    def test_monitor_inconclusive_does_not_establish_native_product_failure(self):
        report = {'status': 'failed', 'monitorFailure': {'unknownKinds': {'permission': 1}}}
        result = validation_outcome(report)
        self.assertEqual(result['execution'], 'monitor-inconclusive')
        self.assertEqual(result['nativeAuthentication'], 'not-verified')
        self.assertFalse(result['monitorFailureEstablishesProductFailure'])
        report['native'] = {'status': 'passed'}
        self.assertEqual(validation_outcome(report)['nativeAuthentication'], 'passed')
        self.assertEqual(report['status'], 'failed')

    def runner(self):
        return Runner(Path(tempfile.gettempdir()), time.time(), {'processCleanupGraceSeconds': 1})

    def test_reap_precedes_probe_and_never_signals_a_reaped_empty_group(self):
        runner = self.runner()
        child = SimpleNamespace(pid=900, returncode=None)
        def poll():
            child.returncode = 0
            return 0
        child.poll = mock.Mock(side_effect=poll)
        def gone(pid, diagnostics):
            self.assertEqual(child.returncode, 0)
            return False
        with mock.patch.object(runner, 'group_alive', side_effect=gone), mock.patch('support.os.killpg') as send:
            runner.stop(child)
        send.assert_not_called()

    def test_exit_between_poll_and_probe_allows_one_fresh_probe_after_exact_reap(self):
        runner = self.runner()
        child = SimpleNamespace(pid=900, returncode=None)
        def poll():
            if child.poll.call_count == 2:
                child.returncode = 0
            return child.returncode
        child.poll = mock.Mock(side_effect=poll)
        with mock.patch.object(runner, 'group_alive', side_effect=[PermissionError(errno.EPERM, 'denied'), False]) as probe:
            self.assertFalse(runner.owned_group_alive(child))
        self.assertEqual(child.returncode, 0)
        self.assertEqual(probe.call_count, 2)

    def test_reap_by_initial_poll_also_requires_authoritative_fresh_group_query(self):
        for fresh in (False, True, PermissionError(errno.EPERM, 'residual')):
            with self.subTest(fresh=type(fresh).__name__):
                runner = self.runner()
                child = SimpleNamespace(pid=900, returncode=None, wait=mock.Mock())
                def poll():
                    child.returncode = 0
                    return 0
                child.poll = mock.Mock(side_effect=poll)
                diagnostics = []
                with mock.patch.object(runner, 'group_alive', side_effect=[PermissionError(errno.EPERM, 'initial'), fresh]) as probe:
                    if isinstance(fresh, PermissionError):
                        with self.assertRaises(PermissionError) as raised:
                            runner.owned_group_alive(child, diagnostics)
                        self.assertIs(raised.exception, fresh)
                    else:
                        self.assertEqual(runner.owned_group_alive(child, diagnostics), fresh)
                    self.assertEqual(probe.call_count, 2)
                child.wait.assert_not_called()
                self.assertEqual(diagnostics[-1]['reapSource'], 'initial-poll')
                self.assertEqual(diagnostics[-1]['reapResult'], 'newly-reaped')
                self.assertTrue(diagnostics[-1]['freshProbeAttempted'])

    def test_live_or_already_reaped_leader_does_not_turn_denial_into_gone(self):
        for initial in (None, 0):
            with self.subTest(returncode=initial):
                runner = self.runner()
                child = SimpleNamespace(pid=900, returncode=initial, poll=mock.Mock(return_value=initial),
                                        wait=mock.Mock(side_effect=subprocess.TimeoutExpired('PRIVATE', 0.05)))
                with mock.patch.object(runner, 'group_alive', side_effect=PermissionError(errno.EPERM, 'denied')) as probe:
                    with self.assertRaises(PermissionError):
                        runner.owned_group_alive(child)
                self.assertEqual(probe.call_count, 1)
                if initial is None:
                    child.wait.assert_called_once_with(timeout=0.05)
                else:
                    child.wait.assert_not_called()

    def test_delayed_exact_reap_permits_one_fresh_probe_but_never_overrides_its_result(self):
        for fresh in (False, True, PermissionError(errno.EPERM, 'residual')):
            with self.subTest(fresh=type(fresh).__name__):
                runner = self.runner()
                denied = PermissionError(errno.EPERM, 'initial')
                child = SimpleNamespace(pid=900, returncode=None, poll=mock.Mock(return_value=None))
                def wait(timeout):
                    self.assertEqual(timeout, 0.05)
                    child.returncode = 7
                    return 7
                child.wait = mock.Mock(side_effect=wait)
                diagnostics = []
                with mock.patch.object(runner, 'group_alive', side_effect=[denied, fresh]) as probe, \
                        mock.patch('support.os.killpg') as signal_group:
                    if isinstance(fresh, PermissionError):
                        with self.assertRaises(PermissionError) as raised:
                            runner.owned_group_alive(child, diagnostics)
                        self.assertIs(raised.exception, fresh)
                    else:
                        self.assertEqual(runner.owned_group_alive(child, diagnostics), fresh)
                    self.assertEqual(probe.call_count, 2)
                    signal_group.assert_not_called()
                child.wait.assert_called_once_with(timeout=0.05)
                self.assertEqual(child.returncode, 7)
                self.assertEqual(diagnostics[-1]['reapSource'], 'bounded-wait')
                self.assertEqual(diagnostics[-1]['reapResult'], 'newly-reaped')
                self.assertTrue(diagnostics[-1]['freshProbeAttempted'])

    def test_delayed_reap_timeout_preserves_original_denial_without_fresh_probe(self):
        runner = self.runner()
        denied = PermissionError(errno.EPERM, 'initial')
        child = SimpleNamespace(pid=900, returncode=None, poll=mock.Mock(return_value=None),
                                wait=mock.Mock(side_effect=subprocess.TimeoutExpired('PRIVATE', 0.05)))
        diagnostics = []
        with mock.patch.object(runner, 'group_alive', side_effect=denied) as probe:
            with self.assertRaises(PermissionError) as raised:
                runner.owned_group_alive(child, diagnostics)
        self.assertIs(raised.exception, denied)
        self.assertEqual(probe.call_count, 1)
        child.wait.assert_called_once_with(timeout=0.05)
        self.assertIsNone(child.returncode)
        self.assertEqual(diagnostics[-1]['reapResult'], 'wait-timeout')
        self.assertFalse(diagnostics[-1]['freshProbeAttempted'])

    def test_wait_without_confirmed_returncode_cannot_permit_fresh_probe(self):
        runner = self.runner()
        denied = PermissionError(errno.EPERM, 'initial')
        child = SimpleNamespace(pid=900, returncode=None, poll=mock.Mock(return_value=None),
                                wait=mock.Mock(return_value=0))
        with mock.patch.object(runner, 'group_alive', side_effect=denied) as probe:
            with self.assertRaises(PermissionError) as raised:
                runner.owned_group_alive(child)
        self.assertIs(raised.exception, denied)
        self.assertEqual(probe.call_count, 1)

    def test_exact_wait_unexpected_error_stays_fail_closed(self):
        runner = self.runner()
        child = SimpleNamespace(pid=900, returncode=None, poll=mock.Mock(return_value=None),
                                wait=mock.Mock(side_effect=OSError(errno.EIO, 'unavailable')))
        with mock.patch.object(runner, 'group_alive', side_effect=PermissionError(errno.EPERM, 'initial')) as probe:
            with self.assertRaises(OSError) as raised:
                runner.owned_group_alive(child)
        self.assertEqual(raised.exception.errno, errno.EIO)
        self.assertEqual(probe.call_count, 1)

    @unittest.skipUnless(sys.platform == 'darwin', 'real Darwin exact-child delayed reap')
    def test_real_mac_controlled_denial_recovers_only_after_exact_child_wait(self):
        from test_mac_recheck import owned_child
        from darwin_activity import probe
        runner = self.runner()
        actual_group_alive = runner.group_alive
        with owned_child() as child:
            state = probe().read(child.pid, child.pid)
            self.assertEqual(state['status'], 'measured')
            self.assertFalse(child_status(child.pid)['exitPending'])
            calls = 0
            def group_alive(pgid, diagnostics=None):
                nonlocal calls
                calls += 1
                if calls == 1:
                    self.assertTrue(actual_group_alive(pgid, diagnostics))
                    child.stdin.write(b'x')
                    child.stdin.flush()
                    raise PermissionError(errno.EPERM, 'controlled denial')
                return actual_group_alive(pgid, diagnostics)
            with mock.patch.object(child, 'poll', return_value=None), \
                    mock.patch.object(child, 'wait', wraps=child.wait) as exact_wait, \
                    mock.patch.object(runner, 'group_alive', side_effect=group_alive):
                self.assertFalse(runner.owned_group_alive(child))
                exact_wait.assert_called_once_with(timeout=0.05)
            self.assertEqual(child.returncode, 0)
            self.assertEqual(calls, 2)
            with self.assertRaises(ChildProcessError):
                os.waitpid(child.pid, os.WNOHANG)
            with self.assertRaises(ProcessLookupError):
                os.killpg(child.pid, 0)
        print(json.dumps({'ownedCleanupRecheck': {'case': 'controlled-delayed-exact-reap',
            'initialDenialInjected': True, 'pollDeferralInjected': True,
            'actualKernelGroupQueries': True, 'liveChildBeforeRelease': True,
            'exactLeaderReapedExitCode': 0, 'postReapGroupProbeErrno': errno.ESRCH,
            'pidTag': state['pidTag'], 'identityTag': state['identityTag']}}), flush=True)

    @unittest.skipUnless(sys.platform == 'darwin', 'real Darwin rapid monitor cleanup')
    def test_real_mac_repeated_monitor_stop_reclaims_owned_group(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        for _ in range(20):
            with tempfile.TemporaryDirectory() as directory:
                runner = Runner(directory, time.time(), limits)
                incomplete = {'sampleComplete': False, 'sampleAttempts': 3,
                              'unknownKinds': {'state-unavailable': 1}}
                with mock.patch.object(runner, 'group_activity', return_value=incomplete), \
                        contextlib.redirect_stdout(io.StringIO()):
                    with self.assertRaisesRegex(Failure, 'monitor-unavailable'):
                        runner.run('rapid-monitor-stop', [sys.executable, '-c', 'import time; time.sleep(3)'],
                                   directory, os.environ.copy(), 360)
                record = runner.records[-1]
                self.assertEqual(runner.failure['code'], 'monitor-unavailable')
                self.assertEqual(record['groupCleanup'], 'verified-gone')
                self.assertEqual(record['leaderReap']['status'], 'reaped')
                self.assertIsNone(record['cleanupFailure'])
        print(json.dumps({'ownedCleanupRecheck': {'case': 'rapid-monitor-stop', 'iterations': 20,
             'groupCleanup': 'verified-gone', 'allLeadersReaped': True}}), flush=True)

    def test_remaining_group_is_checked_and_denied_descendants_stay_unknown(self):
        runner = self.runner()
        child = SimpleNamespace(pid=900, returncode=None)
        def poll():
            if child.poll.call_count == 2:
                child.returncode = 0
            return child.returncode
        child.poll = mock.Mock(side_effect=poll)
        with mock.patch.object(runner, 'group_alive', side_effect=[PermissionError(errno.EPERM, 'denied'), True]):
            self.assertTrue(runner.owned_group_alive(child))
        child.returncode = None
        child.poll.reset_mock()
        with mock.patch.object(runner, 'group_alive', side_effect=[PermissionError(errno.EPERM, 'denied')] * 2):
            with self.assertRaises(PermissionError):
                runner.owned_group_alive(child)

    @unittest.skipUnless(sys.platform == 'darwin', 'real Darwin zombie-only group')
    def test_real_mac_owned_zombie_group_is_gone_only_after_exact_reap(self):
        from darwin_activity import probe
        counter = probe()
        child = subprocess.Popen([sys.executable, '-c', 'import sys; sys.stdin.buffer.read(1)'],
                                 stdin=subprocess.PIPE, start_new_session=True)
        try:
            own = counter.read(child.pid, child.pid)
            self.assertEqual(own['status'], 'measured')
            self.assertFalse(child_status(child.pid)['exitPending'])
            self.assertTrue(self.runner().owned_group_alive(child))
            child.stdin.write(b'x')
            child.stdin.close()
            deadline = time.monotonic() + 5
            while not child_status(child.pid)['exitPending']:
                self.assertLess(time.monotonic(), deadline)
                time.sleep(0.01)
            os.kill(child.pid, 0)
            group_errno = 0
            try:
                os.killpg(child.pid, 0)
            except OSError as error:
                group_errno = error.errno
            state = counter.read(child.pid, child.pid)
            self.assertEqual(state['status'], 'gone')
            self.assertEqual(state['reason'], 'bsd-zombie')
            self.assertEqual(state['identityTag'], own['identityTag'])
            self.assertEqual(group_errno, errno.EPERM)
            self.assertFalse(self.runner().owned_group_alive(child))
            self.assertEqual(child.returncode, 0)
            with self.assertRaises(ProcessLookupError):
                os.killpg(child.pid, 0)
            print(json.dumps({'ownedZombieCleanup': {'pidTag': own['pidTag'], 'identityTag': own['identityTag'],
                  'liveGroupBeforeExit': True, 'unreapedExitPending': True, 'signalZeroSucceeded': True,
                  'unreapedGroupProbeErrno': group_errno, 'bsdZombieIdentityMatched': True,
                  'exactLeaderReapedExitCode': child.returncode, 'postReapGroupProbeErrno': errno.ESRCH,
                  'signalsSent': 0}}), flush=True)
        finally:
            child.stdin.close()
            child.wait(timeout=5)


if __name__ == '__main__':
    unittest.main()

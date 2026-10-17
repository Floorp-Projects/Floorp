# SPDX-License-Identifier: MPL-2.0
import errno
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
from support import Runner
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

    def test_live_or_already_reaped_leader_does_not_turn_denial_into_gone(self):
        for initial in (None, 0):
            with self.subTest(returncode=initial):
                runner = self.runner()
                child = SimpleNamespace(pid=900, returncode=initial, poll=mock.Mock(return_value=initial))
                with mock.patch.object(runner, 'group_alive', side_effect=PermissionError(errno.EPERM, 'denied')) as probe:
                    with self.assertRaises(PermissionError):
                        runner.owned_group_alive(child)
                self.assertEqual(probe.call_count, 1)

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

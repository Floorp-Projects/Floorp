# SPDX-License-Identifier: MPL-2.0
"""Held-anchor contracts. Portable cases never perform OS waits or signals."""
import contextlib
import errno
import io
import json
import math
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

from primary_anchor import Binding, Observation, PrimaryAnchor
from support import Failure, Runner
from pipeline import validation_outcome


class OwnedCleanupGuards(unittest.TestCase):
    def runner(self, directory=None):
        if directory is None:
            temporary = tempfile.TemporaryDirectory()
            self.addCleanup(temporary.cleanup)
            directory = temporary.name
        return Runner(Path(directory), time.time(),
                      {'processCleanupGraceSeconds': 1, 'jobMinutes': 360})

    def anchor(self, *, terminal=True, code=7):
        process = SimpleNamespace(pid=900, returncode=None, stdin=None, stdout=None, stderr=None,
            poll=mock.Mock(side_effect=AssertionError('forbidden-poll')),
            wait=mock.Mock(side_effect=AssertionError('forbidden-wait')),
            send_signal=mock.Mock(side_effect=AssertionError('forbidden-popen-signal')))
        identity = Binding(900, os.getpid(), os.geteuid(), os.getuid(), 900, (123456, 789))
        backend = SimpleNamespace(observe=mock.Mock(return_value=Observation(
            identity, 'zombie' if terminal else 'live', code << 8 if terminal else None)))
        census = mock.Mock(return_value={'status': 'complete', 'group': 900, 'pids': [900],
            'source': 'darwin-header-bound' if sys.platform == 'darwin' else 'linux-ps-test',
            'bindingVerified': True})
        anchor = PrimaryAnchor(process, backend, census)
        anchor.observe()
        return anchor

    def assert_no_popen_reap(self, anchor):
        anchor.process.poll.assert_not_called()
        anchor.process.wait.assert_not_called()
        anchor.process.send_signal.assert_not_called()

    def test_monitor_inconclusive_does_not_establish_native_product_failure(self):
        report = {'status': 'failed', 'monitorFailure': {'unknownKinds': {'permission': 1}}}
        result = validation_outcome(report)
        self.assertEqual(result['execution'], 'monitor-inconclusive')
        self.assertEqual(result['nativeAuthentication'], 'not-verified')
        self.assertFalse(result['monitorFailureEstablishesProductFailure'])
        report['native'] = {'status': 'passed'}
        self.assertEqual(validation_outcome(report)['nativeAuthentication'], 'passed')
        self.assertEqual(report['status'], 'failed')

    def test_raw_popen_cannot_authorize_cleanup(self):
        runner = self.runner()
        child = SimpleNamespace(pid=900, returncode=None, poll=mock.Mock(), wait=mock.Mock())
        with mock.patch('primary_anchor.os.killpg') as send, mock.patch('primary_anchor.os.waitpid') as wait:
            with self.assertRaisesRegex(Failure, '^owned-primary-anchor-required$'):
                runner.stop(child)
        child.poll.assert_not_called()
        child.wait.assert_not_called()
        send.assert_not_called()
        wait.assert_not_called()

    def test_zombie_observation_keeps_direct_child_unreaped(self):
        runner, anchor = self.runner(), self.anchor(code=7)
        with mock.patch('primary_anchor.os.waitpid') as wait, mock.patch('primary_anchor.os.killpg') as send:
            self.assertEqual(runner.observed_exit(anchor), 7)
        wait.assert_not_called()
        send.assert_not_called()
        self.assertIsNone(anchor.actual_returncode)
        self.assertTrue(anchor.held)
        self.assertEqual(anchor.receipt()['resources'], {'rssMiB': None, 'cpuSeconds': None})
        self.assert_no_popen_reap(anchor)

    def test_zombie_retirement_uses_exact_wait_without_numeric_probe_or_signal(self):
        runner, anchor, diagnostics = self.runner(), self.anchor(code=7), []
        with mock.patch('primary_anchor.os.waitpid', return_value=(900, 7 << 8)) as wait, \
                mock.patch('primary_anchor.os.killpg') as send, \
                mock.patch.object(runner, 'group_alive', side_effect=AssertionError('numeric-probe')):
            runner.stop(anchor, diagnostics)
        wait.assert_called_once_with(900, os.WNOHANG)
        send.assert_not_called()
        self.assertEqual(anchor.actual_returncode, 7)
        self.assertEqual(anchor.lifecycle_state, 'reaped')
        self.assertEqual(diagnostics[-1]['operation'], 'anchor-retirement')
        self.assertIsNone(anchor.receipt()['groupGone'])
        self.assert_no_popen_reap(anchor)

    def test_denied_observation_stays_unknown_without_numeric_probe(self):
        runner, anchor, diagnostics = self.runner(), self.anchor(terminal=False), []
        anchor._backend.observe.side_effect = PermissionError(errno.EPERM, 'PRIVATE_DENIAL')
        with mock.patch('primary_anchor.os.waitpid') as wait, mock.patch('primary_anchor.os.killpg') as send, \
                mock.patch.object(runner, 'group_alive', side_effect=AssertionError('numeric-probe')):
            with self.assertRaisesRegex(Failure, '^owned-primary-signal-unavailable$'):
                runner.stop(anchor, diagnostics)
        wait.assert_not_called()
        send.assert_not_called()
        self.assertIsNone(anchor.actual_returncode)
        self.assertEqual((diagnostics[-1]['errorType'], diagnostics[-1]['errno']), ('os-error', errno.EPERM))
        self.assertIs(diagnostics[-1]['signalAuthority'], False)
        self.assertNotIn('PRIVATE_DENIAL', json.dumps(diagnostics))
        self.assert_no_popen_reap(anchor)

    def test_unknown_observation_does_not_synthesize_exit_zero(self):
        runner, anchor, diagnostics = self.runner(), self.anchor(terminal=False), []
        anchor._backend.observe.side_effect = OSError(errno.EIO, 'PRIVATE_UNAVAILABLE')
        with mock.patch('primary_anchor.os.waitpid') as wait, mock.patch('primary_anchor.os.killpg') as send:
            with self.assertRaisesRegex(Failure, '^owned-primary-observation-unavailable$'):
                runner.observed_exit(anchor, diagnostics)
        wait.assert_not_called()
        send.assert_not_called()
        self.assertIsNone(anchor.actual_returncode)
        self.assertEqual(diagnostics[-1]['errno'], errno.EIO)

    def test_unknown_raw_census_is_not_empty_and_cannot_reap(self):
        anchor = self.anchor(code=0)
        anchor._census.side_effect = PermissionError(errno.EACCES, 'PRIVATE_CENSUS')
        with mock.patch('primary_anchor.os.waitpid') as wait, mock.patch('primary_anchor.os.killpg') as send:
            result = anchor.try_reap(deadline=time.monotonic() + 1)
        wait.assert_not_called()
        send.assert_not_called()
        self.assertIsNone(result['actualReturncode'])
        self.assertIsNone(result['groupGone'])
        self.assertEqual((result['errorType'], result['errno']), ('os-error', errno.EACCES))
        self.assertTrue(anchor.held)

    def test_nonleader_raw_roster_blocks_exact_primary_reap(self):
        anchor = self.anchor(code=0)
        anchor._census.return_value['pids'] = [900, 901]
        with mock.patch('primary_anchor.os.waitpid') as wait, mock.patch('primary_anchor.os.killpg') as send:
            result = anchor.try_reap()
        self.assertEqual(result['reason'], 'anchor-nonleaders-remain')
        wait.assert_not_called()
        send.assert_not_called()
        self.assertIsNone(anchor.actual_returncode)

    def test_live_held_signal_then_exact_reap_preserves_exit(self):
        runner, anchor, diagnostics = self.runner(), self.anchor(terminal=False), []
        identity = anchor._binding
        def signal_owned(group, value):
            self.assertEqual((group, value), (900, int(signal.SIGINT)))
            self.assertTrue(anchor.held)
            self.assertIsNone(anchor.actual_returncode)
            anchor._backend.observe.return_value = Observation(identity, 'zombie', 7 << 8)
        with mock.patch('primary_anchor.os.killpg', side_effect=signal_owned) as send, \
                mock.patch('primary_anchor.os.waitpid', return_value=(900, 7 << 8)) as wait:
            runner.stop(anchor, diagnostics)
        self.assertEqual(send.call_count, 1)
        wait.assert_called_once_with(900, os.WNOHANG)
        self.assertEqual(anchor.actual_returncode, 7)
        signal_records = [item for item in diagnostics if item['operation'] == 'anchor-group-signal']
        self.assertEqual(len(signal_records), 1)
        self.assertEqual(signal_records[0]['signal'], int(signal.SIGINT))
        self.assert_no_popen_reap(anchor)

    def test_retirement_blocks_signals_even_if_numeric_group_reappears(self):
        runner, anchor = self.runner(), self.anchor(code=0)
        with mock.patch('primary_anchor.os.waitpid', return_value=(900, 0)):
            runner.stop(anchor)
        anchor._census.return_value['pids'] = [901]
        with mock.patch('primary_anchor.os.waitpid') as wait, mock.patch('primary_anchor.os.killpg') as send, \
                mock.patch.object(runner, 'group_alive', side_effect=AssertionError('numeric-probe')):
            runner.stop(anchor)
            self.assertIs(anchor.signal_group(int(signal.SIGKILL))['signalAuthority'], False)
            self.assertIs(anchor.post_reap_absence()['groupEmpty'], False)
        wait.assert_not_called()
        send.assert_not_called()
        self.assert_no_popen_reap(anchor)

    def test_denied_postreap_read_does_not_restore_signal_authority(self):
        runner, anchor = self.runner(), self.anchor(code=0)
        with mock.patch('primary_anchor.os.waitpid', return_value=(900, 0)):
            runner.stop(anchor)
        anchor._census.side_effect = PermissionError(errno.EPERM, 'PRIVATE_RETIRED')
        with mock.patch('primary_anchor.os.waitpid') as wait, mock.patch('primary_anchor.os.killpg') as send:
            absence = anchor.post_reap_absence()
            runner.stop(anchor)
        self.assertIsNone(absence['groupEmpty'])
        self.assertFalse(absence['signalAuthority'])
        self.assertEqual(absence['errno'], errno.EPERM)
        wait.assert_not_called()
        send.assert_not_called()

    def test_exact_wait_mismatch_fails_closed_without_rewait(self):
        runner, anchor = self.runner(), self.anchor(code=0)
        with mock.patch('primary_anchor.os.waitpid', return_value=(900, 7 << 8)) as wait, \
                mock.patch('primary_anchor.os.killpg') as send:
            with self.assertRaisesRegex(Failure, '^owned-primary-anchor-unavailable$'):
                runner.stop(anchor)
            anchor.recover_exact_child()
        wait.assert_called_once_with(900, os.WNOHANG)
        send.assert_not_called()
        self.assertEqual(anchor.actual_returncode, 7)
        self.assertTrue(anchor.receipt()['actualReaped'])
        self.assertEqual(anchor.receipt()['reason'], 'anchor-wait-status-mismatch')
        self.assertFalse(anchor.receipt()['signalAuthority'])

    def test_echild_does_not_synthesize_zero_or_permit_signal(self):
        runner, anchor = self.runner(), self.anchor(code=0)
        with mock.patch('primary_anchor.os.waitpid', side_effect=ChildProcessError(errno.ECHILD, 'PRIVATE')) as wait, \
                mock.patch('primary_anchor.os.killpg') as send:
            with self.assertRaisesRegex(Failure, '^owned-primary-anchor-unavailable$'):
                runner.stop(anchor)
            self.assertEqual(anchor.recover_exact_child()['reason'], 'anchor-echild')
            with self.assertRaisesRegex(Failure, '^owned-primary-anchor-unavailable$'):
                runner.stop(anchor)
        self.assertEqual(wait.call_count, 1)
        send.assert_not_called()
        self.assertIsNone(anchor.actual_returncode)
        self.assert_no_popen_reap(anchor)

    def test_cleanup_denial_preserves_existing_primary_failure(self):
        runner, anchor = self.runner(), self.anchor(terminal=False)
        first = {'stage': 'functional-stage', 'code': 'nonzero-exit'}
        runner.failure = first
        anchor._backend.observe.side_effect = PermissionError(errno.EPERM, 'PRIVATE')
        with mock.patch('primary_anchor.os.waitpid') as wait, mock.patch('primary_anchor.os.killpg') as send:
            with self.assertRaises(Failure):
                runner.stop(anchor, [])
        self.assertIs(runner.failure, first)
        wait.assert_not_called()
        send.assert_not_called()

    def test_first_retirement_loss_keeps_stopping_reason_and_errno(self):
        runner, anchor, diagnostics = self.runner(), self.anchor(code=0), []
        with mock.patch('primary_anchor.os.waitpid', side_effect=ChildProcessError(errno.ECHILD, 'PRIVATE_LOSS')) as wait, \
                mock.patch('primary_anchor.os.killpg') as send:
            with self.assertRaisesRegex(Failure, '^owned-primary-anchor-unavailable$'):
                runner.stop(anchor, diagnostics)
        wait.assert_called_once_with(900, os.WNOHANG)
        send.assert_not_called()
        self.assertEqual(diagnostics[0]['operation'], 'anchor-retirement')
        self.assertEqual((diagnostics[0]['reason'], diagnostics[0]['errorType'], diagnostics[0]['errno']),
                         ('anchor-echild', 'os-error', errno.ECHILD))
        self.assertIsNone(diagnostics[0]['actualReturncode'])
        self.assertNotIn('PRIVATE_LOSS', json.dumps(diagnostics))

    def test_entry_lost_stop_records_refusal_without_fresh_query_or_os_operation(self):
        runner, anchor, diagnostics = self.runner(), self.anchor(code=0), []
        anchor._revoke('anchor-echild')
        anchor._set_error(ChildProcessError(errno.ECHILD, 'PRIVATE_ALREADY_LOST'))
        anchor._backend.observe.reset_mock()
        anchor._census.reset_mock()
        with mock.patch('primary_anchor.os.waitpid') as wait, mock.patch('primary_anchor.os.killpg') as send:
            with self.assertRaisesRegex(Failure, '^owned-primary-anchor-unavailable$'):
                runner.stop(anchor, diagnostics)
        wait.assert_not_called()
        send.assert_not_called()
        anchor._backend.observe.assert_not_called()
        anchor._census.assert_not_called()
        self.assertEqual(len(diagnostics), 1)
        self.assertEqual((diagnostics[0]['operation'], diagnostics[0]['reason'], diagnostics[0]['errorType'],
                          diagnostics[0]['errno']), ('anchor-stop-refused', 'anchor-echild', 'os-error', errno.ECHILD))
        self.assertIs(diagnostics[0]['signalAuthority'], False)
        self.assert_no_popen_reap(anchor)

    def test_expired_original_deadline_permits_no_cleanup_operation(self):
        runner, anchor = self.runner(), self.anchor(terminal=False)
        with mock.patch.object(runner, 'anchor_deadline', return_value=0), \
                mock.patch('primary_anchor.os.waitpid') as wait, mock.patch('primary_anchor.os.killpg') as send:
            with self.assertRaisesRegex(Failure, '^owned-process-cleanup-timeout$'):
                runner.stop(anchor)
        wait.assert_not_called()
        send.assert_not_called()
        self.assertIsNone(anchor.actual_returncode)

    def mac_runner(self, directory):
        # Missing strict ABI/origin fails, never skips or substitutes resource data.
        self.assertIn('FLOORP_CENSUS_ABI', os.environ, 'owned-cleanup-native-abi-required')
        abi = Path(os.environ['FLOORP_CENSUS_ABI'])
        self.assertTrue(abi.is_file() and not abi.is_symlink(), 'owned-cleanup-native-abi-file')
        self.assertIn('TASK_ROOT', os.environ, 'owned-cleanup-task-origin-required')
        marker = Path(os.environ['TASK_ROOT']) / 'started-monotonic'
        origin = float(marker.read_text())
        self.assertTrue(math.isfinite(origin) and 0 < origin <= time.monotonic())
        Path(directory, 'started-monotonic').write_text(str(origin))
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        return Runner(directory, time.time(), limits)

    @contextlib.contextmanager
    def mac_child(self, runner):
        anchor = PrimaryAnchor.spawn([sys.executable, '-c', 'import sys;sys.stdin.buffer.read(1)'],
            stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            census=lambda pgid, *, deadline=None: runner.anchor_inventory(
                pgid, 'owned-cleanup-mac-control', deadline=deadline))
        try:
            yield anchor
        finally:
            if anchor.process.stdin is not None and not anchor.process.stdin.closed:
                anchor.close_input()
            if anchor.held:
                runner.stop(anchor)
            self.assertEqual(anchor.lifecycle_state, 'reaped')
            self.assertIs(type(anchor.actual_returncode), int)
            absence = anchor.post_reap_absence(deadline=runner.anchor_deadline(5))
            self.assertEqual((absence['status'], absence['groupEmpty']), ('observed', True))

    def mac_zombie(self, runner, anchor):
        deadline = runner.anchor_deadline(5)
        while True:
            value = anchor.observe(deadline=deadline)
            if value['status'] == 'zombie':
                return deadline
            self.assertEqual(value['status'], 'live', 'owned-cleanup-zombie-observation-unavailable')
            self.assertLess(time.monotonic(), deadline)
            time.sleep(min(.01, max(0, deadline-time.monotonic())))

    @unittest.skipUnless(sys.platform == 'darwin', 'real Darwin held-child controlled denial')
    def test_real_mac_controlled_denial_keeps_unreaped_child_and_no_signal(self):
        with tempfile.TemporaryDirectory() as directory:
            runner = self.mac_runner(directory)
            with self.mac_child(runner) as anchor:
                self.assertEqual(anchor.observe()['status'], 'live')
                diagnostics = []
                with mock.patch.object(anchor._backend.reader, 'query', return_value=(0, errno.EPERM)), \
                        mock.patch('primary_anchor.os.killpg', side_effect=AssertionError('denial-signal')), \
                        mock.patch('primary_anchor.os.waitpid', side_effect=AssertionError('denial-wait')):
                    with self.assertRaisesRegex(Failure, '^owned-primary-signal-unavailable$'):
                        runner.stop(anchor, diagnostics)
                self.assertIsNone(anchor.actual_returncode)
                self.assertEqual(diagnostics[-1]['errno'], errno.EPERM)
                self.assertEqual(anchor.observe()['status'], 'live')
                anchor.close_input()
                self.mac_zombie(runner, anchor)
                runner.stop(anchor)
                self.assertEqual(anchor.actual_returncode, 0)
        print(json.dumps({'ownedCleanupRecheck': {'case': 'held-controlled-denial',
            'initialDenialInjected': True, 'pollOrWaitUsed': False, 'denialAuthorizedSignal': False,
            'nativeCensusRequired': True, 'exactLeaderReapedExitCode': 0}}), flush=True)

    @unittest.skipUnless(sys.platform == 'darwin', 'real Darwin rapid held-anchor cleanup')
    def test_real_mac_twenty_rapid_monitor_stops_reclaim_owned_groups(self):
        for _ in range(20):
            with tempfile.TemporaryDirectory() as directory:
                runner = self.mac_runner(directory)
                incomplete = {'sampleComplete': False, 'sampleAttempts': 3,
                              'unknownKinds': {'state-unavailable': 1}}
                with mock.patch.object(runner, 'group_activity', return_value=incomplete), \
                        contextlib.redirect_stdout(io.StringIO()):
                    with self.assertRaisesRegex(Failure, 'monitor-unavailable'):
                        runner.run('rapid-monitor-stop', [sys.executable, '-c', 'import time;time.sleep(30)'],
                                   directory, os.environ.copy(), runner.limits['jobMinutes'])
                record = runner.records[-1]
                self.assertEqual(runner.failure['code'], 'monitor-unavailable')
                self.assertEqual(record['groupCleanup'], 'verified-gone')
                self.assertEqual(record['leaderReap']['status'], 'reaped')
                self.assertIsNone(record['cleanupFailure'])
                self.assertTrue(record['primaryAnchor']['retirementVerified'])
                self.assertTrue(record['primaryAnchor']['actualReaped'])
                self.assertTrue(record['primaryAnchor']['signalAuthorityRevoked'])
                self.assertIs(record['primaryAnchor']['postReapAbsence']['groupEmpty'], True)
                self.assertEqual(record['primaryAnchor']['sessionOrDescendantClosure'], 'unconfirmed')
        print(json.dumps({'ownedCleanupRecheck': {'case': 'rapid-held-monitor-stop', 'iterations': 20,
            'groupCleanup': 'verified-gone', 'allExactLeadersReaped': True,
            'nativeCensusRequired': True, 'sessionOrDescendantClosure': 'unconfirmed'}}), flush=True)

    @unittest.skipUnless(sys.platform == 'darwin', 'real Darwin retained zombie raw census')
    def test_real_mac_zombie_resource_gone_is_not_raw_group_absence(self):
        from darwin_activity import probe
        with tempfile.TemporaryDirectory() as directory:
            runner = self.mac_runner(directory)
            with self.mac_child(runner) as anchor:
                self.assertEqual(anchor.observe()['status'], 'live')
                identity = anchor._binding
                anchor.close_input()
                deadline = self.mac_zombie(runner, anchor)
                self.assertEqual(anchor._binding, identity)
                self.assertIsNone(anchor.actual_returncode)
                raw = runner.anchor_inventory(anchor.pid, 'owned-cleanup-mac-control', deadline=deadline)
                self.assertIn(anchor.pid, raw['pids'])
                resources = probe().read(anchor.pid, anchor.pid)
                self.assertEqual((resources['status'], resources['reason']), ('gone', 'bsd-zombie'))
                self.assertEqual(anchor.try_reap(deadline=deadline)['status'], 'reaped')
                self.assertEqual(anchor.actual_returncode, 0)
                self.assertTrue(anchor.post_reap_absence(deadline=deadline)['groupEmpty'])
        print(json.dumps({'ownedZombieCleanup': {'positiveZombieIdentityMatched': True,
            'resourceGoneDidNotMeanRawAbsent': True, 'rawRosterIncludedZombie': True,
            'exactLeaderReapedExitCode': 0, 'postReapReadOnlyAbsence': True}}), flush=True)


if __name__ == '__main__':
    unittest.main()

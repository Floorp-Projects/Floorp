# SPDX-License-Identifier: MPL-2.0
"""Portable final-wait models and guarded surfaces: no child creation, signals, or waits."""
from contextlib import ExitStack
import errno
import os
import signal
import sys
import threading
import time
import unittest
from types import SimpleNamespace
from unittest import mock

import primary_anchor as primary


class AnchorWaitModels(unittest.TestCase):
    def setUp(self):
        self.registry = dict(primary._registry)
        self.opaque = dict(primary._opaque_registry)
        self.forbidden = []
        for name in ('kill', 'killpg', 'waitpid'):
            patch = mock.patch.object(os, name, side_effect=AssertionError('unexpected-os-' + name))
            self.forbidden.append(patch.start())
            self.addCleanup(patch.stop)
        patch = mock.patch.object(primary.GuardedPopen, '__new__',
                                  side_effect=AssertionError('unexpected-real-popen'))
        patch.start()
        self.addCleanup(patch.stop)

    def tearDown(self):
        primary._registry.clear()
        primary._registry.update(self.registry)
        primary._opaque_registry.clear()
        primary._opaque_registry.update(self.opaque)
        for call in self.forbidden:
            call.assert_not_called()

    def anchor(self, observed=7):
        hidden = {name: mock.Mock(side_effect=AssertionError('hidden-popen-' + name))
                  for name in ('poll', 'wait', 'send_signal', 'communicate', '_internal_poll',
                               '_wait', '_try_wait', '_communicate', '__enter__', '__exit__', '__del__')}
        process = SimpleNamespace(pid=900, returncode=None, stdin=None, stdout=None, stderr=None, **hidden)
        binding = primary.Binding(900, os.getpid(), os.geteuid(), os.getuid(), 900, (123456, 789))
        observation = primary.Observation(binding, 'zombie', observed << 8)
        backend = SimpleNamespace(observe=mock.Mock(return_value=observation))
        census = mock.Mock(return_value={'status': 'complete', 'group': 900, 'pids': [900],
            'source': 'darwin-header-bound' if sys.platform == 'darwin' else 'linux-ps-test',
            'bindingVerified': True})
        anchor = primary.PrimaryAnchor(process, backend, census)
        self.assertEqual(anchor.observe()['status'], 'zombie')
        return anchor

    def assert_hidden_unused(self, anchor):
        for name in ('poll', 'wait', 'send_signal', 'communicate', '_internal_poll',
                     '_wait', '_try_wait', '_communicate', '__enter__', '__exit__', '__del__'):
            getattr(anchor.process, name).assert_not_called()

    def assert_revoked_unknown(self, anchor, result):
        self.assertEqual(result['state'], 'lost')
        self.assertIsNone(result['actualReturncode'])
        self.assertIsNone(anchor.actual_returncode)
        self.assertIs(result['signalAuthority'], False)
        self.assertFalse(anchor.held)
        self.assertIs(anchor.signal_group(int(signal.SIGKILL))['signalAuthority'], False)
        self.assert_hidden_unused(anchor)

    def test_actual_guarded_popen_surfaces_refuse_without_child_creation(self):
        active = list(primary.subprocess._active)
        metrics = primary.METRICS.copy()
        process = object.__new__(primary.GuardedPopen)
        process.returncode = None
        # This object never enters Popen.__init__ and represents no OS child.
        surfaces = (
            ('poll', ()), ('wait', ()), ('send_signal', (int(signal.SIGTERM),)),
            ('communicate', ()), ('__enter__', ()), ('__exit__', (None, None, None)),
            ('_internal_poll', ()), ('_try_wait', (os.WNOHANG,)), ('_wait', (0,)),
            ('_communicate', (None, None, None)), ('terminate', ()), ('kill', ()),
        )
        calls = []
        with ExitStack() as stack:
            # setUp already forbids kill/killpg/waitpid and ordinary Popen allocation.
            for target, name in ((primary.subprocess.Popen, '__init__'),
                                 (primary.subprocess.Popen, '_execute_child'),
                                 (primary.subprocess, '_fork_exec'),
                                 *((os, name) for name in
                                   ('fork', 'posix_spawn', 'posix_spawnp', 'wait', 'waitid')
                                   if hasattr(os, name))):
                if hasattr(target, name):
                    calls.append(stack.enter_context(mock.patch.object(
                        target, name, side_effect=AssertionError('unexpected-process-operation'))))
            try:
                process.arm()
                for name, args in surfaces:
                    with self.subTest(surface=name):
                        with self.assertRaises(primary.AnchorError) as caught:
                            getattr(process, name)(*args)
                        self.assertEqual(caught.exception.args, ('anchor-hidden-reap-forbidden',))
                self.assertEqual(primary.METRICS['forbiddenPopenCalls'] -
                                 metrics['forbiddenPopenCalls'], len(surfaces))
            finally:
                # Fixture-only finalizer sentinel; no exit/cleanup outcome is claimed.
                process.returncode = 0
                process.__del__()
                primary.METRICS.clear()
                primary.METRICS.update(metrics)
            self.assertEqual(primary.subprocess._active, active)
            for call in calls:
                call.assert_not_called()

    def test_wait_zero_means_unconfirmed_not_exit_zero(self):
        anchor = self.anchor()
        with mock.patch.object(os, 'waitpid', return_value=(0, 0)) as wait:
            result = anchor.try_reap()
        wait.assert_called_once_with(900, os.WNOHANG)
        self.assertEqual(result['reason'], 'anchor-wait-not-ready')
        self.assertIsNone(result['actualReaped'])
        self.assert_revoked_unknown(anchor, result)

    def test_wrong_wait_pid_does_not_prove_an_exact_reap(self):
        for pid in (901, -1):
            with self.subTest(pid=pid):
                anchor = self.anchor()
                with mock.patch.object(os, 'waitpid', return_value=(pid, 0)):
                    result = anchor.try_reap()
                self.assertEqual(result['reason'], 'anchor-wait-pid-mismatch')
                self.assertIsNone(result['actualReaped'])
                self.assert_revoked_unknown(anchor, result)

    def test_bool_wait_pid_is_not_zero_or_positive_pid(self):
        for pid in (False, True):
            with self.subTest(pid=pid):
                anchor = self.anchor()
                with mock.patch.object(os, 'waitpid', return_value=(pid, 0)):
                    result = anchor.try_reap()
                self.assertEqual(result['reason'], 'anchor-wait-pid-mismatch')
                self.assertIsNone(result['actualReaped'])
                self.assert_revoked_unknown(anchor, result)

    def test_bool_raw_status_is_unknown_after_irreversible_exact_wait(self):
        for raw in (False, True):
            with self.subTest(raw=raw):
                anchor = self.anchor()
                with mock.patch.object(os, 'waitpid', return_value=(900, raw)) as wait:
                    result = anchor.try_reap()
                    recovered = anchor.recover_exact_child()
                self.assertEqual(wait.call_count, 1)
                self.assertTrue(result['actualReaped'])
                self.assertEqual(result['reason'], 'anchor-wait-status')
                self.assertEqual(recovered, result)
                self.assert_revoked_unknown(anchor, result)

    def test_noncanonical_raw_status_cannot_become_real_returncode(self):
        for raw in (0x0709, 0x137f, 0xffff):
            with self.subTest(raw=raw):
                anchor = self.anchor()
                with mock.patch.object(os, 'waitpid', return_value=(900, raw)) as wait:
                    result = anchor.try_reap()
                    anchor.recover_exact_child()
                self.assertEqual(wait.call_count, 1)
                self.assertTrue(result['actualReaped'])
                self.assertEqual(result['reason'], 'anchor-wait-status')
                self.assert_revoked_unknown(anchor, result)

    def test_echild_revokes_and_never_waits_or_signals_again(self):
        anchor = self.anchor()
        with mock.patch.object(os, 'waitpid', side_effect=ChildProcessError(errno.ECHILD, 'PRIVATE')) as wait:
            result = anchor.try_reap()
            recovered = anchor.recover_exact_child()
        self.assertEqual(wait.call_count, 1)
        self.assertEqual((result['reason'], result['errorType'], result['errno']),
                         ('anchor-echild', 'os-error', errno.ECHILD))
        self.assertEqual(recovered['reason'], 'anchor-echild')
        self.assert_revoked_unknown(anchor, result)

    def test_eio_final_wait_remains_unknown_and_revoked(self):
        anchor = self.anchor()
        def unavailable(pid, flags):
            self.assertEqual(anchor.lifecycle_state, 'reaping')
            self.assertFalse(anchor.receipt()['signalAuthority'])
            raise OSError(errno.EIO, 'PRIVATE')
        with mock.patch.object(os, 'waitpid', side_effect=unavailable):
            result = anchor.try_reap()
        self.assertEqual((result['reason'], result['errorType'], result['errno']),
                         ('anchor-final-wait-unavailable', 'os-error', errno.EIO))
        self.assert_revoked_unknown(anchor, result)

    def test_single_eintr_retries_same_expected_pid_and_original_deadline(self):
        anchor = self.anchor()
        deadline = time.monotonic() + 2
        calls = []
        def interrupted(pid, flags):
            calls.append((pid, flags))
            self.assertEqual(anchor.lifecycle_state, 'reaping')
            self.assertFalse(anchor.receipt()['signalAuthority'])
            if len(calls) == 1:
                raise InterruptedError(errno.EINTR, 'PRIVATE')
            return pid, 7 << 8
        with mock.patch.object(primary, 'check_deadline', wraps=primary.check_deadline) as checks, \
                mock.patch.object(os, 'waitpid', side_effect=interrupted):
            result = anchor.try_reap(deadline=deadline)
        self.assertEqual(calls, [(900, os.WNOHANG), (900, os.WNOHANG)])
        self.assertTrue(all(call.args == (deadline,) for call in checks.call_args_list))
        anchor._census.assert_called_once_with(900, deadline=deadline)
        self.assertEqual((result['status'], result['actualReturncode']), ('reaped', 7))
        self.assertTrue(result['actualReaped'])
        self.assertIsNone(result['errorType'])
        self.assert_hidden_unused(anchor)

    def test_two_eintr_are_bounded_and_never_synthesize_a_status(self):
        anchor = self.anchor()
        with mock.patch.object(os, 'waitpid', side_effect=InterruptedError(errno.EINTR, 'PRIVATE')) as wait:
            result = anchor.try_reap(deadline=time.monotonic() + 2)
        self.assertEqual(wait.call_count, 2)
        self.assertEqual(result['reason'], 'anchor-final-wait-interrupted')
        self.assert_revoked_unknown(anchor, result)

    def test_expiration_before_second_wait_prevents_a_new_grace(self):
        anchor = self.anchor()
        original = primary.check_deadline
        deadline = time.monotonic() + 2
        reaping_checks = []
        def expires(value):
            self.assertEqual(value, deadline)
            original(value)
            if anchor.lifecycle_state == 'reaping':
                reaping_checks.append(value)
                if len(reaping_checks) == 2:
                    raise primary.AnchorError('anchor-operation-deadline')
        with mock.patch.object(primary, 'check_deadline', side_effect=expires), \
                mock.patch.object(os, 'waitpid', side_effect=InterruptedError(errno.EINTR, 'PRIVATE')) as wait:
            result = anchor.try_reap(deadline=deadline)
        self.assertEqual(wait.call_count, 1)
        self.assertEqual(reaping_checks, [deadline, deadline])
        self.assertEqual(result['reason'], 'anchor-operation-deadline')
        self.assert_revoked_unknown(anchor, result)

    def test_reentrant_final_observation_has_no_nested_signal_or_wait(self):
        anchor = self.anchor()
        original = anchor._backend.observe.return_value
        nested = []
        def observe(*args, **kwargs):
            nested.extend([anchor.try_reap(), anchor.signal_group(int(signal.SIGKILL)),
                           anchor.recover_exact_child()])
            return original
        anchor._backend.observe.side_effect = observe
        with mock.patch.object(os, 'waitpid', return_value=(900, 7 << 8)) as wait:
            result = anchor.try_reap()
        self.assertEqual(wait.call_count, 1)
        self.assertEqual(len(nested), 6)
        self.assertTrue(all(value['reason'] == 'anchor-operation-busy' for value in nested))
        self.assertTrue(all(value['signalAuthority'] is False for value in nested))
        self.assertEqual((result['status'], anchor.actual_returncode), ('reaped', 7))
        self.assert_hidden_unused(anchor)

    def test_competing_final_waits_have_one_exact_wait_and_no_hidden_popen_calls(self):
        anchor = self.anchor()
        entered, release, second_started = threading.Event(), threading.Event(), threading.Event()
        results, errors = [], []
        def waited(pid, flags):
            self.assertEqual((pid, flags), (900, os.WNOHANG))
            self.assertEqual(anchor.lifecycle_state, 'reaping')
            entered.set()
            self.assertTrue(release.wait(2))
            return pid, 7 << 8
        def run(second=False):
            if second:
                second_started.set()
            try:
                results.append(anchor.try_reap())
            except BaseException as error:
                errors.append(type(error).__name__)
        threads = [threading.Thread(target=run), threading.Thread(target=run, args=(True,))]
        with mock.patch.object(os, 'waitpid', side_effect=waited) as wait:
            try:
                threads[0].start()
                self.assertTrue(entered.wait(2))
                threads[1].start()
                self.assertTrue(second_started.wait(2))
            finally:
                release.set()
                for thread in threads:
                    if thread.ident is not None:
                        thread.join(2)
            self.assertEqual(wait.call_count, 1)
        self.assertFalse(any(thread.is_alive() for thread in threads))
        self.assertEqual(errors, [])
        self.assertEqual(len(results), 2)
        self.assertTrue(all(value['status'] == 'reaped' for value in results))
        self.assertEqual(anchor.actual_returncode, 7)
        self.assert_hidden_unused(anchor)

    def test_mismatch_retains_real_code_and_irreversible_reap_fact(self):
        anchor = self.anchor(observed=0)
        with mock.patch.object(os, 'waitpid', return_value=(900, 7 << 8)) as wait:
            result = anchor.try_reap()
            self.assertEqual(anchor.recover_exact_child(), result)
            self.assertEqual(anchor.try_reap(), result)
        wait.assert_called_once_with(900, os.WNOHANG)
        self.assertEqual((result['state'], result['reason'], result['actualReturncode']),
                         ('lost', 'anchor-wait-status-mismatch', 7))
        self.assertTrue(result['actualReaped'])
        self.assertIs(result['signalAuthority'], False)
        self.assertIs(anchor.signal_group(int(signal.SIGKILL))['signalAuthority'], False)
        self.assert_hidden_unused(anchor)


if __name__ == '__main__':
    unittest.main()

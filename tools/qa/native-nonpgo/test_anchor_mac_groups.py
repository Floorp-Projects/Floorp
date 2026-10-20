# SPDX-License-Identifier: MPL-2.0
"""Actual Darwin group boundaries; explicitly select after the bound ABI bootstrap.

The controller exactly waits its primary child. Grandchildren are disposed through
an inherited private EOF channel and observed absent through the raw producer;
their launchd reaping is never represented as a controller exact wait.
"""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

import primary_anchor as primary
from anchor_inventory import collect


ORIGINAL_WAITPID = os.waitpid


@unittest.skipUnless(sys.platform == 'darwin', 'actual Darwin group fixtures')
class DarwinGroupTests(unittest.TestCase):
    def setUp(self):
        self.assertIn('FLOORP_CENSUS_ABI', os.environ, 'anchor-mac-abi-required')
        self.temp = tempfile.TemporaryDirectory(prefix='floorp-anchor-groups-')
        self.root = Path(self.temp.name)
        self.anchors = []
        self.groups = set()

    def until(self, predicate, deadline=None):
        deadline = time.monotonic() + 8 if deadline is None else deadline
        while time.monotonic() < deadline:
            value = predicate()
            if value:
                return value
            time.sleep(.01)
        self.fail('anchor-fixture-deadline')

    def census(self, pgid, *, deadline=None):
        return collect(pgid, deadline=deadline)

    def spawn(self, code, *, backend=None):
        try:
            child = primary.PrimaryAnchor.spawn([sys.executable, '-c', code],
                stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                census=self.census, backend=backend)
        except (primary.AnchorSpawnError, primary.AnchorInterrupted) as error:
            if error.anchor is not None:
                self.anchors.append(error.anchor)
                if type(error.anchor.pid) is int:
                    self.groups.add(error.anchor.pid)
            raise
        self.anchors.append(child)
        self.groups.add(child.pid)
        return child

    def zombie(self, anchor):
        def observed():
            value = anchor.observe(deadline=time.monotonic() + 3)
            self.assertIn(value['status'], ('live', 'zombie'))
            return value if value['status'] == 'zombie' else None
        return self.until(observed)

    def raw_members(self, group):
        return self.census(group, deadline=time.monotonic() + 3)['pids']

    def leaf(self, pid, group, birth=None, state=None):
        from darwin_activity import probe
        status, info, _reason = probe().bsd(pid)
        self.assertEqual(status, 'gone' if state == 5 else 'measured')
        self.assertEqual((info.pid, info.pgid, info.uid, info.ruid),
                         (pid, group, os.geteuid(), os.getuid()))
        self.assertGreater(info.start_sec, 0)
        self.assertTrue(0 <= info.start_usec < 1000000)
        current = (info.start_sec, info.start_usec)
        if birth is not None:
            self.assertEqual(current, birth)
        if state is not None:
            self.assertEqual(info.status, state)
        return current

    def evidence(self, case, **fields):
        print(json.dumps({'anchorMacGroups': {'case': case, **fields,
            'controllerWaitScope': 'direct-primary-only',
            'scope': 'active-process-group-epoch',
            'sessionOrDescendantClosure': 'unconfirmed',
            'rawProcessMetadataPublished': False}}), flush=True)

    def tearDown(self):
        errors = []
        # EOF is a retained private channel even after the group authority ends.
        for anchor in self.anchors:
            try:
                anchor.close_input()
            except Exception as error:
                errors.append(type(error).__name__)
        for anchor in self.anchors:
            try:
                deadline = time.monotonic() + 8
                while anchor.actual_returncode is None and time.monotonic() < deadline:
                    if anchor.held:
                        anchor.try_reap(deadline=deadline)
                    else:
                        anchor.recover_exact_child(deadline=deadline)
                    if anchor.actual_returncode is None:
                        time.sleep(.01)
                self.assertIsNotNone(anchor.actual_returncode)
                with self.assertRaises(ChildProcessError):
                    ORIGINAL_WAITPID(anchor.pid, os.WNOHANG)
                if anchor.lifecycle_state == 'reaped':
                    self.assertTrue(anchor.post_reap_absence(deadline=deadline)['groupEmpty'])
            except Exception as error:
                errors.append(type(error).__name__)
            finally:
                for name in ('stdin', 'stdout', 'stderr'):
                    try:
                        stream = getattr(anchor.process, name, None)
                        if stream is not None and not stream.closed:
                            stream.close()
                    except Exception as error:
                        errors.append(type(error).__name__)
        for group in self.groups:
            try:
                self.until(lambda: not self.raw_members(group))
            except Exception as error:
                errors.append(type(error).__name__)
        self.temp.cleanup()
        self.assertEqual(errors, [], 'actual fixture cleanup remains unconfirmed')

    def fork_code(self, mode):
        # Files are private test handshakes, never ownership authority for a
        # signal. Every group signal goes through the unreaped primary anchor.
        return '''import json,os,pathlib,signal,sys,time
root=pathlib.Path(%r)
mode=%r
def ready(path):
    limit=time.monotonic()+6
    while not path.exists():
        if time.monotonic()>=limit: os._exit(121)
        time.sleep(.005)
pid=os.fork()
if pid==0:
    if mode in ('leave','rejoin'):
        os.setpgid(0,0)
    if mode=='zombie':
        os._exit(7)
    (root/'leaf-ready').touch()
    if mode=='rejoin':
        command=sys.stdin.buffer.read(1)
        if command==b'R':
            os.setpgid(0,os.getsid(0))
            (root/'rejoined').touch()
    sys.stdin.buffer.read()
    os._exit(0)
(root/'leaf-pid').write_text(str(pid))
if mode=='zombie':
    (root/'leader-ready').touch()
    sys.stdin.buffer.read(1)
    waited,status=os.waitpid(pid,0)
    (root/'delegated-wait').write_text(json.dumps({'matched':waited==pid,'status':status}))
    os._exit(0)
ready(root/'leaf-ready')
os._exit(0)
''' % (str(self.root), mode)

    def leaf_pid(self):
        self.until(lambda: (self.root / 'leaf-pid').is_file())
        pid = int((self.root / 'leaf-pid').read_text())
        self.assertTrue(0 < pid < 2**31)
        return pid

    def test_canonical_os_exit_arguments_match_actual_wait(self):
        cases = (0, 7, 255, 256, 65535, -1, 2147483647, -2147483648)
        for argument in cases:
            with self.subTest(argument=argument):
                child = self.spawn('import os;os._exit(%d)' % argument)
                observed = self.zombie(child)
                expected = argument & 255
                self.assertEqual(observed['observedExitCode'], expected)
                native = child._backend.observe(child.pid, child.pid, child._binding)
                actual = []
                def exact(pid, flags):
                    value = ORIGINAL_WAITPID(pid, flags)
                    if pid == child.pid:
                        actual.append(value)
                    return value
                with mock.patch.object(os, 'waitpid', side_effect=exact):
                    result = child.try_reap(deadline=time.monotonic() + 3)
                self.assertEqual(actual, [(child.pid, native.raw_status)])
                self.assertEqual((result['status'], result['actualReturncode']), ('reaped', expected))
                self.assertEqual(result['resources'], {'cpuSeconds': None, 'rssMiB': None})
        self.evidence('canonical-os-exit-arguments', fixedArguments=list(cases), matchedWaits=8)

    def test_live_nonleader_blocks_reap_after_primary_exit(self):
        child = self.spawn(self.fork_code('live'))
        leaf = self.leaf_pid()
        self.zombie(child)
        birth = self.leaf(leaf, child.pid)
        self.assertIn(leaf, self.raw_members(child.pid))
        before = child.try_reap(deadline=time.monotonic() + 3)
        self.assertEqual(before['reason'], 'anchor-nonleaders-remain')
        self.assertIsNone(child.actual_returncode)
        self.assertTrue(child.held)
        self.leaf(leaf, child.pid, birth=birth)
        sent = child.signal_group(int(signal.SIGTERM), deadline=time.monotonic() + 3)
        self.assertEqual(sent['reason'], 'anchor-signal-sent')
        self.until(lambda: self.raw_members(child.pid) == [child.pid])
        self.assertEqual(child.try_reap(deadline=time.monotonic() + 3)['status'], 'reaped')
        self.assertEqual(child.actual_returncode, 0)
        self.assertTrue(child.post_reap_absence(deadline=time.monotonic() + 3)['groupEmpty'])
        self.evidence('leader-zombie-nonleader-live', earlyReapBlocked=True,
            groupSignalWhilePrimaryUnreaped=True, grandchildDisposition='raw-absence', directMatchedWait=True)

    def test_first_positive_binding_can_already_be_zombie(self):
        from darwin_activity import BSDInfo, probe
        native = primary.DarwinBSD(probe(), BSDInfo, os.getpid(), os.geteuid(), os.getuid())
        first_states = []
        case = self
        class InitiallyTerminal:
            def observe(self, pid, group, expected=None, *, deadline=None):
                end = time.monotonic() + 5 if deadline is None else deadline
                while time.monotonic() < end:
                    observed = native.observe(pid, group, expected, deadline=end)
                    if observed.state == 'zombie':
                        first_states.append(observed.state)
                        return observed
                    time.sleep(.002)
                case.fail('initially-terminal-fixture-deadline')
        child = self.spawn('import os;os._exit(7)', backend=InitiallyTerminal())
        self.assertEqual(first_states, ['zombie'])
        self.assertEqual(child.observed_exit_code, 7)
        self.assertIsNone(child.actual_returncode)
        self.assertEqual(child.try_reap(deadline=time.monotonic() + 3)['actualReturncode'], 7)
        self.assertEqual(child.receipt()['resources'], {'cpuSeconds': None, 'rssMiB': None})
        self.evidence('initially-fast-exited', firstPositiveState='zombie', actualReturncode=7,
            unmeasuredResourceValues=None)

    def test_raw_nonleader_zombie_requires_delegated_parent_wait(self):
        child = self.spawn(self.fork_code('zombie'))
        leaf = self.leaf_pid()
        self.until(lambda: (self.root / 'leader-ready').exists())
        from darwin_activity import probe
        def terminal():
            status, info, _reason = probe().bsd(leaf)
            return status == 'gone' and info is not None and info.status == 5
        self.until(terminal)
        birth = self.leaf(leaf, child.pid, state=5)
        raw = self.raw_members(child.pid)
        self.assertEqual(sorted(raw), sorted((child.pid, leaf)))
        sample = probe().read(leaf, child.pid)
        self.assertEqual((sample['status'], sample['reason']), ('gone', 'bsd-zombie'))
        self.assertIsNone(sample.get('rssMiB'))
        self.assertEqual(child.observe()['status'], 'live')
        self.assertIsNone(child.actual_returncode)
        self.leaf(leaf, child.pid, birth=birth, state=5)
        child.process.stdin.write(b'W')
        child.process.stdin.flush()
        self.until(lambda: (self.root / 'delegated-wait').is_file())
        acknowledgement = json.loads((self.root / 'delegated-wait').read_text())
        self.assertEqual(acknowledgement, {'matched': True, 'status': 7 << 8})
        self.zombie(child)
        self.assertEqual(self.raw_members(child.pid), [child.pid])
        self.assertEqual(child.try_reap(deadline=time.monotonic() + 3)['status'], 'reaped')
        self.evidence('nonleader-zombie-is-member', rawZombieSeen=True,
            resourceExcludedZombie=True, delegatedMatchedWait=True, directMatchedWait=True)

    def test_same_session_leave_does_not_establish_descendant_closure(self):
        child = self.spawn(self.fork_code('leave'))
        leaf = self.leaf_pid()
        self.groups.add(leaf)
        self.zombie(child)
        birth = self.leaf(leaf, leaf)
        self.assertEqual(os.getsid(leaf), child.pid)
        self.assertEqual(self.raw_members(child.pid), [child.pid])
        self.assertEqual(child.try_reap(deadline=time.monotonic() + 3)['status'], 'reaped')
        self.assertTrue(child.post_reap_absence(deadline=time.monotonic() + 3)['groupEmpty'])
        self.leaf(leaf, leaf, birth=birth)
        self.assertEqual(child.receipt()['sessionOrDescendantClosure'], 'unconfirmed')
        with mock.patch.object(os, 'killpg', side_effect=AssertionError('retired-signal')):
            self.assertFalse(child.signal_group(int(signal.SIGTERM))['signalAuthority'])
        child.close_input()
        self.until(lambda: not self.raw_members(leaf))
        self.evidence('same-session-leave', groupAbsentWhileEscapedMemberLive=True,
            descendantClosure='unconfirmed', grandchildDisposition='private-EOF-then-raw-absence')

    def test_same_session_rejoin_between_snapshot_and_wait_stays_read_only(self):
        child = self.spawn(self.fork_code('rejoin'))
        leaf = self.leaf_pid()
        self.groups.add(leaf)
        self.zombie(child)
        birth = self.leaf(leaf, leaf)
        self.assertEqual(os.getsid(leaf), child.pid)
        def snapshot_then_rejoin(group, *, deadline=None):
            snapshot = self.census(group, deadline=deadline)
            self.assertEqual(snapshot['pids'], [child.pid])
            child.process.stdin.write(b'R')
            child.process.stdin.flush()
            self.until(lambda: (self.root / 'rejoined').exists(), deadline=deadline)
            self.leaf(leaf, child.pid, birth=birth)
            return snapshot
        self.assertEqual(child.try_reap(snapshot_then_rejoin, deadline=time.monotonic() + 5)['status'], 'reaped')
        self.assertEqual(child.actual_returncode, 0)
        with mock.patch.object(os, 'killpg', side_effect=AssertionError('rejoin-after-retirement-signal')):
            absence = child.post_reap_absence(deadline=time.monotonic() + 3)
            self.assertEqual((absence['status'], absence['groupEmpty'], absence['signalAuthority']),
                             ('observed', False, False))
            self.assertFalse(child.signal_group(int(signal.SIGTERM))['signalAuthority'])
        child.close_input()
        self.until(lambda: not self.raw_members(child.pid))
        self.evidence('same-session-rejoin-before-wait', snapshotThenRejoin=True,
            postReapGroupEmpty=False, postReapSignalCalls=0,
            grandchildDisposition='private-EOF-then-raw-absence')

    def test_actual_final_wait_echild_never_becomes_zero_or_authority(self):
        child = self.spawn('pass')
        self.zombie(child)
        external = []
        def external_reap_then_actual_wait(pid, flags):
            if pid != child.pid:
                return ORIGINAL_WAITPID(pid, flags)
            external.append(ORIGINAL_WAITPID(pid, flags))
            return ORIGINAL_WAITPID(pid, flags)  # The actual kernel now returns ECHILD.
        with mock.patch.object(os, 'waitpid', side_effect=external_reap_then_actual_wait):
            result = child.try_reap(deadline=time.monotonic() + 3)
        self.assertEqual(external, [(child.pid, 0)])
        self.assertEqual(result['reason'], 'anchor-echild')
        self.assertIsNone(result['actualReturncode'])
        self.assertIsNot(result['actualReaped'], True)
        with mock.patch.object(os, 'waitpid', side_effect=AssertionError('echild-recovery-wait')), \
                mock.patch.object(os, 'killpg', side_effect=AssertionError('echild-signal')):
            self.assertFalse(child.signal_group(int(signal.SIGTERM))['signalAuthority'])
            self.assertIsNone(child.recover_exact_child()['actualReturncode'])
        # Fixture-only matched external wait is not an anchor retirement proof.
        child.process.returncode = os.waitstatus_to_exitcode(external[0][1])
        primary._registry.pop(child.pid, None)
        self.evidence('actual-final-wait-echild', actualKernelECHILD=True,
            externalMatchedWait=True, anchorActualReturncode=None,
            retirementVerified=False, postECHILDWaitCalls=0, postECHILDSignalCalls=0)

    def test_live_resource_sample_does_not_turn_terminal_resources_into_zero(self):
        child = self.spawn('import sys;sys.stdin.buffer.read()')
        from darwin_activity import probe
        self.assertEqual(child.observe()['status'], 'live')
        counter = probe()
        live_status, live_info, _reason = counter.bsd(child.pid)
        self.assertEqual(live_status, 'measured')
        self.assertEqual((live_info.pid, live_info.pgid), (child.pid, child.pid))
        self.assertEqual(live_info.xstatus & 65535, 0)
        self.assertEqual(child.observe()['status'], 'live')
        sample = counter.read(child.pid, child.pid)
        self.assertEqual(sample['status'], 'measured')
        self.assertIsInstance(sample['rssMiB'], (int, float))
        self.assertIsInstance(sample['cpuSeconds'], (int, float))
        self.assertGreaterEqual(sample['rssMiB'], 0)
        self.assertGreaterEqual(sample['cpuSeconds'], 0)
        pid, sec, usec = sample['_birth']
        time.sleep(.01)
        second = counter.read(child.pid, child.pid,
                              expected={'pid': pid, 'startSec': sec, 'startUsec': usec})
        self.assertEqual(second['status'], 'measured')
        self.assertEqual(second['_birth'], sample['_birth'])
        self.assertGreaterEqual(second['rssMiB'], 0)
        self.assertGreaterEqual(second['cpuSeconds'], sample['cpuSeconds'])
        child.close_input()
        self.zombie(child)
        terminal = probe().read(child.pid, child.pid)
        self.assertEqual((terminal['status'], terminal['reason']), ('gone', 'bsd-zombie'))
        self.assertIsNone(terminal.get('rssMiB'))
        self.assertIsNone(terminal.get('cpuSeconds'))
        self.assertEqual(child.try_reap(deadline=time.monotonic() + 3)['status'], 'reaped')
        self.assertEqual(child.receipt()['resources'], {'cpuSeconds': None, 'rssMiB': None})
        self.evidence('live-resources-then-terminal', liveResourcesMeasured=True, liveResourceSamples=2,
            liveLowStatusZeroObserved=True, liveLowStatusZeroTreatedAsExit=False,
            terminalResourceValues=None, terminalZeroResourcesFabricated=False)

    def test_term_and_kill_bsd_status_match_actual_primary_wait(self):
        for requested in (signal.SIGTERM, signal.SIGKILL):
            with self.subTest(requested=int(requested)):
                child = self.spawn('import sys;sys.stdin.buffer.read()')
                self.assertEqual(child.observe()['status'], 'live')
                self.assertEqual(child.signal_group(int(requested))['reason'], 'anchor-signal-sent')
                self.assertEqual(self.zombie(child)['observedExitCode'], -int(requested))
                native = child._backend.observe(child.pid, child.pid, child._binding)
                actual = []
                def exact(pid, flags):
                    value = ORIGINAL_WAITPID(pid, flags)
                    if pid == child.pid:
                        actual.append(value)
                    return value
                with mock.patch.object(os, 'waitpid', side_effect=exact):
                    result = child.try_reap(deadline=time.monotonic() + 3)
                self.assertEqual(actual, [(child.pid, native.raw_status)])
                self.assertEqual(result['actualReturncode'], -int(requested))
                self.assertFalse(child.receipt()['signalAuthority'])
        self.evidence('signal-status', requestedSignals=[15, 9], canonicalRawStatusMatched=True,
            directMatchedWaits=2, postReapSignalCalls=0)

    def test_query_timeout_reaps_its_child_while_real_primary_remains_held(self):
        from anchor_inventory import InventoryError
        child = self.spawn('import sys;sys.stdin.buffer.read()')
        self.assertEqual(child.observe()['status'], 'live')
        binding = child._binding
        query_children, query_groups = [], []
        run, popen = subprocess.run, subprocess.Popen
        def query_popen(*args, **kwargs):
            query = popen(*args, **kwargs)
            query_children.append(query)
            query_groups.append(os.getpgid(query.pid))
            return query
        def stalled_producer(*args, **kwargs):
            self.assertGreater(kwargs['timeout'], 0)
            self.assertLessEqual(kwargs['timeout'], 1)
            return run([sys.executable, '-c', 'import time;time.sleep(30)'], **kwargs)
        try:
            with mock.patch('anchor_inventory.subprocess.run', side_effect=stalled_producer), \
                    mock.patch('subprocess.Popen', side_effect=query_popen):
                with self.assertRaisesRegex(InventoryError, 'anchor-inventory-timeout'):
                    self.census(child.pid, deadline=time.monotonic() + 1)
            self.assertEqual(len(query_children), 1)
            query = query_children[0]
            self.assertIsInstance(query.returncode, int)
            self.assertEqual(query_groups, [os.getpgrp()])
            self.assertNotEqual(query_groups[0], child.pid)
            with self.assertRaises(ChildProcessError):
                ORIGINAL_WAITPID(query.pid, os.WNOHANG)
            for stream in (query.stdout, query.stderr):
                self.assertTrue(stream.closed)
            self.assertEqual(child.observe()['status'], 'live')
            self.assertEqual(child._binding, binding)
            self.assertIsNone(child.actual_returncode)
            self.assertTrue(child.held)
            self.assertIn(child.pid, self.raw_members(child.pid))
            self.evidence('bounded-owned-query-timeout', queryFailureInjected=True,
                actualNativeKernelQuery=False, exactQueryChildReaped=True,
                heldPrimaryStayedLive=True, authorityFromTimeout=False)
        finally:
            # The wrapper can fail after Popen returned but before run() receives
            # its handle (for example getpgid raises). Retain and dispose each
            # exact query child independently of the primary anchor.
            cleanup_errors = []
            for query in query_children:
                if query.returncode is None:
                    try:
                        query.kill()
                    except BaseException as error:
                        cleanup_errors.append(type(error).__name__)
                    try:
                        query.wait(timeout=5)
                    except BaseException as error:
                        cleanup_errors.append(type(error).__name__)
                for name in ('stdin', 'stdout', 'stderr'):
                    try:
                        stream = getattr(query, name, None)
                        if stream is not None and not stream.closed:
                            stream.close()
                    except BaseException as error:
                        cleanup_errors.append(type(error).__name__)
            self.assertEqual(cleanup_errors, [], 'query fixture cleanup remains unconfirmed')


if __name__ == '__main__':
    unittest.main()

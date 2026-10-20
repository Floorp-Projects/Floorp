import ctypes
from dataclasses import replace
import errno
import gc
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import threading
import time
import unittest
from unittest import mock

import primary_anchor as p

CREATED = []
EXTERNAL_REAPS = {}
FIXTURE_REAPS = 0
ORIGINAL_WAITPID = os.waitpid
ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('unchanged_darwin_activity', ROOT/'darwin_activity.py')
darwin = importlib.util.module_from_spec(spec)
spec.loader.exec_module(darwin)


def wait_zombie(anchor, seconds=2):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        value = anchor.observe()
        if value['status'] == 'zombie':
            return value
        if anchor._state != 'held':
            break
        time.sleep(.002)
    raise AssertionError('fixture-terminal-timeout')



def fixture_stop(child,deadline):
    if child._busy:
        return child.receipt(reason='anchor-operation-busy',signalAuthority=False)
    if not child.held:
        return child.receipt()
    sent=False
    while time.monotonic()<deadline:
        value=child.try_reap(deadline=deadline)
        if not child.held:return value
        if not sent:
            value=child.signal_group(int(signal.SIGKILL),deadline=deadline)
            sent=value['reason']=='anchor-signal-sent'
        time.sleep(.002)
    return child.receipt(reason='anchor-drain-deadline')


def constructor_gap_control(case, before_pid_assignment):
    """Real isolated direct child; fixture-only PID proof is never helper authority."""
    import inspect
    captured={};anchor=None;previous_trace=sys.gettrace()
    original_fork=subprocess._fork_exec
    code=subprocess.Popen._execute_child.__code__
    lines,start=inspect.getsourcelines(subprocess.Popen._execute_child)
    target=next(start+i for i,line in enumerate(lines) if line.strip()=='self._child_created = True')
    def trace(frame,event,arg):
        if frame.f_code is code and event=='line' and frame.f_lineno==target and not captured:
            process=frame.f_locals['self'];captured.update(process=process,pid=process.pid)
            raise KeyboardInterrupt('fixed-pid-before-child-flag')
        return trace
    def fork_then_interrupt(*args,**kwargs):
        process=inspect.currentframe().f_back.f_locals['self']
        pid=original_fork(*args,**kwargs)
        captured.update(process=process,pid=pid)
        raise KeyboardInterrupt('fixed-fork-return-before-pid-store')
    try:
        context=mock.patch.object(subprocess,'_fork_exec',new=fork_then_interrupt)
        if not before_pid_assignment:sys.settrace(trace)
        with context if before_pid_assignment else mock.patch.object(sys,'gettrace',wraps=sys.gettrace):
            with case.assertRaises(p.AnchorInterrupted) as caught:
                p.PrimaryAnchor.spawn([sys.executable,'-c','import sys;sys.stdin.buffer.read()'],
                    stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,
                    census=case.census if hasattr(case,'census') else p.LinuxProcfsCensus())
        anchor=caught.exception.anchor
        case.assertIsNotNone(anchor);case.assertTrue(captured)
        process=captured['process'];case.assertIs(anchor.process,process)
        case.assertTrue(process._armed);case.assertFalse(process._child_created)
        case.assertIsNone(anchor.actual_returncode)
        if before_pid_assignment:
            case.assertIsNone(anchor.pid)
            case.assertIs(p._opaque_registry[id(process)],anchor)
            case.assertEqual(anchor.receipt()['childExistence'],'unconfirmed')
        else:
            case.assertEqual(anchor.pid,captured['pid'])
            case.assertIs(p._registry[captured['pid']],anchor)
        with mock.patch.object(os,'waitpid',side_effect=AssertionError('unbound-wait')), \
                mock.patch.object(os,'killpg',side_effect=AssertionError('constructor-group-signal')):
            case.assertEqual(anchor.recover_exact_child()['reason'],'anchor-recovery-unbound')
            case.assertFalse(anchor.signal_group(int(signal.SIGKILL))['signalAuthority'])
        with case.assertWarnsRegex(ResourceWarning,'^anchor-unresolved-quarantine$'):
            process.__del__()
    finally:
        sys.settrace(previous_trace)
        if captured:
            process,pid=captured['process'],captured['pid'];CREATED.append(pid)
            # The fixture captured the real fork return. It confirms that exact own
            # direct child without relying on the unknown helper PID or a PGID.
            if sys.platform=='linux':
                terminal=os.waitid(os.P_PID,pid,os.WEXITED|os.WNOWAIT|os.WNOHANG) is not None
            else:
                probe=p.DarwinBSD(darwin.probe(),darwin.BSDInfo,os.getpid(),os.geteuid(),os.getuid())
                terminal=probe.observe(pid,pid).state=='zombie'
            if not terminal:os.kill(pid,signal.SIGKILL)
            waited,raw=ORIGINAL_WAITPID(pid,0);case.assertEqual(waited,pid)
            process.returncode=os.waitstatus_to_exitcode(raw)
            for name in ('stdin','stdout','stderr'):
                stream=getattr(process,name,None)
                if stream is not None and not stream.closed:stream.close()
            p._registry.pop(pid,None);p._opaque_registry.pop(id(process),None)

@unittest.skipUnless(sys.platform=="linux", "Linux actual-child tests")
class LinuxActualTests(unittest.TestCase):
    def setUp(self):
        self.children = []
        self.extras = []

    def spawn(self, script='import sys; sys.stdin.buffer.read(); sys.exit(7)', **kwargs):
        child = p.PrimaryAnchor.spawn([sys.executable, '-c', script], backend=p.LinuxWNOWAIT(),
                               census=p.LinuxProcfsCensus(),stdin=subprocess.PIPE,
                               stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,**kwargs)
        self.children.append(child)
        CREATED.append(child._pid)
        return child

    def tearDown(self):
        global FIXTURE_REAPS
        # Fixture rescue only knows actual Popen handles it directly created.
        # No numeric cached PGID signal is used here, including lost models.
        for process in self.extras:
            if process.returncode is None:
                try:
                    os.kill(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                try:
                    pid, raw = ORIGINAL_WAITPID(process.pid, 0)
                    self.assertEqual(pid, process.pid)
                    process.returncode = os.waitstatus_to_exitcode(raw)
                    FIXTURE_REAPS += 1
                except ChildProcessError:
                    self.fail('fixture-extra-reap-unaccounted')
            for stream in (process.stdin,process.stdout,process.stderr):
                if stream is not None and not stream.closed:
                    stream.close()
        for anchor in self.children:
            anchor._census = p.LinuxProcfsCensus()
            anchor._backend = p.LinuxWNOWAIT()
            if anchor._state == 'held':
                result = fixture_stop(anchor,time.monotonic()+2)
                self.assertEqual(result['status'], 'reaped', result)
            if anchor._process.returncode is None:
                # Tests explicitly check unresolved-warning behavior before rescue.
                with self.assertWarnsRegex(ResourceWarning, '^anchor-unresolved-quarantine$'):
                    anchor._process.warn_unresolved()
                if anchor._pid in EXTERNAL_REAPS:
                    raw = EXTERNAL_REAPS.pop(anchor._pid)
                else:
                    try:
                        os.kill(anchor._pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    waited, raw = ORIGINAL_WAITPID(anchor._pid, 0)
                    self.assertEqual(waited, anchor._pid)
                    FIXTURE_REAPS += 1
                # Fixture-only recorded real wait, never authorizes primitive reuse.
                anchor._process.returncode = os.waitstatus_to_exitcode(raw)
            anchor._close_input()
            p._registry.pop(anchor._pid, None)
            self.assertFalse(anchor._state == 'lost' and anchor.receipt()['signalAuthority'])
        gc.collect()

    def test_fast_exit_0(self):
        self.check_exit(0)

    def test_fast_exit_7(self):
        self.check_exit(7)

    def test_fast_exit_255(self):
        self.check_exit(255)

    def check_exit(self, code):
        child = self.spawn(f'import sys; sys.exit({code})')
        observed = wait_zombie(child)
        self.assertEqual(observed['observedExitCode'], code)
        self.assertIsNone(child._process.returncode)
        for _ in range(3):
            self.assertEqual(child.observe()['observedExitCode'], code)
        result = child.try_reap()
        self.assertEqual((result['status'], result['exitCode'], result['actualReaped']),
                         ('reaped', code, True))
        self.assertEqual(child._process.returncode, code)
        self.assertTrue(child.post_reap_absence()['groupEmpty'])

    def test_eof_exit_and_unmeasured_resources(self):
        child = self.spawn()
        self.assertIsNone(child.receipt()['resources']['cpuSeconds'])
        self.assertIsNone(child.receipt()['resources']['rssMiB'])
        child.close_input()
        wait_zombie(child)
        self.assertEqual(child.try_reap()['exitCode'], 7)

    def test_real_signal_exit(self):
        child = self.spawn()
        result = child.signal_group(int(signal.SIGTERM))
        self.assertEqual(result['reason'], 'anchor-signal-sent')
        self.assertEqual(wait_zombie(child)['observedExitCode'], -signal.SIGTERM)
        self.assertEqual(child.try_reap()['exitCode'], -signal.SIGTERM)

    def test_repeated_stop_and_foreign_reuse_model_no_signal(self):
        child = self.spawn()
        self.assertEqual(fixture_stop(child,time.monotonic()+2)['status'], 'reaped')
        child._census = lambda group,**kwargs: {'status': 'complete', 'group': group,
            'pids': [group+100000], 'source': 'linux-procfs-test', 'bindingVerified': True}
        with mock.patch.object(os, 'killpg', side_effect=AssertionError('foreign-signal')):
            self.assertEqual(fixture_stop(child,time.monotonic()+2)['status'], 'reaped')
            self.assertEqual(child.signal_group(int(signal.SIGKILL))['reason'],
                             'anchor-authority-revoked')
            self.assertFalse(child.post_reap_absence()['groupEmpty'])
        self.assertFalse(child.receipt()['signalAuthority'])

    def test_all_hidden_popen_reaping_calls_rejected(self):
        child = self.spawn()
        proc = child._process
        calls = [proc.poll, proc.wait, lambda: proc.send_signal(signal.SIGTERM),
                 proc.communicate, proc.__enter__, lambda: proc.__exit__(None,None,None),
                 proc._internal_poll, lambda: proc._try_wait(os.WNOHANG),
                 lambda: proc._wait(timeout=0),
                 lambda: proc._communicate(None,None,None), proc.terminate, proc.kill]
        for call in calls:
            with self.subTest(call=call):
                with self.assertRaisesRegex(p.AnchorError, '^anchor-hidden-reap-forbidden$'):
                    call()
        self.assertIsNone(proc.returncode)

    def test_live_child_cannot_be_reaped(self):
        child = self.spawn()
        with mock.patch.object(os, 'waitpid', side_effect=AssertionError('early-wait')):
            self.assertEqual(child.try_reap()['reason'], 'anchor-not-terminal')

    def test_unknown_census_retains_anchor(self):
        child = self.spawn('pass')
        wait_zombie(child)
        child._census = lambda group,**kwargs: None
        with mock.patch.object(os, 'waitpid', side_effect=AssertionError('unknown-reap')):
            self.assertEqual(child.try_reap()['reason'], 'anchor-census-schema')
        self.assertIsNone(child._process.returncode)

    def test_empty_census_is_not_anchor_only(self):
        child = self.spawn('pass')
        wait_zombie(child)
        child._census = lambda group,**kwargs: {'status': 'complete', 'group': group,
            'pids': [], 'source': 'linux-procfs-test', 'bindingVerified': True}
        with mock.patch.object(os, 'waitpid', side_effect=AssertionError('empty-reap')):
            self.assertEqual(child.try_reap()['reason'], 'anchor-missing-from-roster')


    def test_external_reaper_echild_irreversibly_revokes(self):
        child = self.spawn('pass')
        wait_zombie(child)
        waited, raw = ORIGINAL_WAITPID(child._pid, 0)
        self.assertEqual(waited, child._pid)
        EXTERNAL_REAPS[child._pid] = raw
        result = child.observe()
        self.assertEqual((result['state'], result['reason'], result['exitCode']),
                         ('lost','anchor-echild',None))
        self.assertIsNone(child._process.returncode)
        with mock.patch.object(os, 'killpg', side_effect=AssertionError('lost-signal')):
            fixture_stop(child,time.monotonic()+1)
            child.signal_group(int(signal.SIGKILL))

    def test_other_reaper_between_positive_observation_and_wait(self):
        child = self.spawn('pass')
        wait_zombie(child)
        def compete(pid, flags):
            _, raw = ORIGINAL_WAITPID(pid, flags)
            EXTERNAL_REAPS[pid] = raw
            raise ChildProcessError(errno.ECHILD, 'private-error-sentinel')
        with mock.patch.object(os, 'waitpid', side_effect=compete):
            value = child.try_reap()
        self.assertEqual(value['reason'], 'anchor-echild')
        self.assertIsNone(value['exitCode'])
        self.assertNotIn('private-error-sentinel', json.dumps(value))

    def test_generation_mismatch_revokes_without_signals(self):
        child = self.spawn()
        actual = child._backend.observe(child._pid, child._pid, child._binding)
        changed = replace(actual, binding=replace(actual.binding,birth=(actual.binding.birth[0]+1,)))
        with mock.patch.object(child._backend, 'observe', return_value=changed), \
                mock.patch.object(os, 'killpg', side_effect=AssertionError('changed-signal')):
            self.assertEqual(child.signal_group(int(signal.SIGKILL))['reason'],
                             'anchor-generation-changed')
        # Fixture owns exact direct child and releases EOF, not cached PGID cleanup.
        child._close_input()

    def test_wait_raw_mismatch_reaps_but_fails_proof(self):
        child = self.spawn('pass')
        wait_zombie(child)
        def wrong_status(pid, flags):
            waited, raw = ORIGINAL_WAITPID(pid, flags)
            return waited, raw ^ 256
        with mock.patch.object(os, 'waitpid', side_effect=wrong_status):
            value = child.try_reap()
        self.assertEqual((value['state'],value['reason'],value['actualReaped'],value['exitCode']),
                         ('lost','anchor-wait-status-mismatch',True,None))
        with mock.patch.object(os, 'killpg', side_effect=AssertionError('mismatch-signal')):
            child.signal_group(int(signal.SIGKILL))

    def test_positive_wait_with_malformed_status_is_not_zero(self):
        child=self.spawn('pass');wait_zombie(child)
        def malformed(pid,flags):
            waited,raw=ORIGINAL_WAITPID(pid,flags)
            EXTERNAL_REAPS[pid]=raw
            return waited,True
        with mock.patch.object(os,'waitpid',side_effect=malformed):
            result=child.try_reap()
        self.assertEqual((result['state'],result['reason'],result['actualReaped'],result['exitCode']),
                         ('lost','anchor-wait-status',True,None))
        self.assertIsNone(child._process.returncode)

    def test_wait_zero_is_not_exit_zero(self):
        child = self.spawn('pass')
        wait_zombie(child)
        with mock.patch.object(os, 'waitpid', return_value=(0,0)):
            value = child.try_reap()
        self.assertEqual(value['reason'], 'anchor-wait-not-ready')
        self.assertIsNone(value['exitCode'])

    def test_wait_unknown_oserror_revokes_before_return(self):
        child=self.spawn('pass')
        wait_zombie(child)
        def bad_wait(*args):
            self.assertEqual(child._state,'reaping')
            self.assertFalse(child.receipt()['signalAuthority'])
            raise OSError(errno.EIO,'private-wait-error')
        with mock.patch.object(os,'waitpid',side_effect=bad_wait):
            value=child.try_reap()
        self.assertEqual((value['state'],value['reason'],value['exitCode']),
                         ('lost','anchor-final-wait-unavailable',None))
        with mock.patch.object(os,'killpg',side_effect=AssertionError('after-unknown-wait')):
            child.signal_group(int(signal.SIGKILL))

    def test_wait_eintr_limited_retry_with_original_deadline(self):
        child=self.spawn('pass')
        wait_zombie(child)
        count=0
        def interrupted(pid,flags):
            nonlocal count
            count+=1
            if count==1:
                raise InterruptedError(errno.EINTR,'private-interrupt')
            return ORIGINAL_WAITPID(pid,flags)
        with mock.patch.object(os,'waitpid',side_effect=interrupted):
            self.assertEqual(child.try_reap(deadline=time.monotonic()+1)['status'],'reaped')
        self.assertEqual(count,2)

    def test_wait_two_eintr_revokes(self):
        child=self.spawn('pass')
        wait_zombie(child)
        with mock.patch.object(os,'waitpid',side_effect=InterruptedError(errno.EINTR,'private')) as call:
            value=child.try_reap(deadline=time.monotonic()+1)
        self.assertEqual(call.call_count,2)
        self.assertEqual((value['state'],value['reason']),('lost','anchor-final-wait-interrupted'))

    def test_wait_retry_never_resets_deadline(self):
        child=self.spawn('pass')
        wait_zombie(child)
        checks=[]
        def checked(deadline):
            checks.append(deadline)
            if child._state=='reaping' and len([x for x in checks if x==deadline])>5:
                raise p.AnchorError('anchor-operation-deadline')
        # Dedicated original deadline callback, not sleeping to trigger a race.
        end=time.monotonic()+1
        original_check=p.check_deadline
        calls=0
        def expiration(deadline):
            nonlocal calls
            original_check(deadline)
            if child._state=='reaping':
                calls+=1
                self.assertEqual(deadline,end)
                if calls==2:
                    raise p.AnchorError('anchor-operation-deadline')
        with mock.patch.object(p,'check_deadline',side_effect=expiration), \
                mock.patch.object(os,'waitpid',side_effect=InterruptedError(errno.EINTR,'private')) as wait:
            result=child.try_reap(deadline=end)
        self.assertEqual(wait.call_count,1)
        self.assertEqual((result['state'],result['reason']),('lost','anchor-operation-deadline'))


    def test_arbitrary_backend_failure_text_not_projected(self):
        child=self.spawn()
        with mock.patch.object(child._backend,'observe',side_effect=p.AnchorError('/private/sentinel')):
            value=child.observe()
        self.assertEqual(value['reason'],'anchor-observation-unavailable')
        self.assertNotIn('sentinel',json.dumps(value))

    def test_unavailable_query_never_publishes_cached_signal_authority(self):
        child=self.spawn()
        self.assertIsNotNone(child._binding)
        with mock.patch.object(child._backend,'observe',side_effect=p.AnchorError('anchor-bsd-unavailable')), \
                mock.patch.object(os,'killpg',side_effect=AssertionError('unavailable-signal')):
            value=child.observe()
            self.assertIs(value['signalAuthority'],False)
            self.assertIs(value['freshIdentityVerified'],False)
            self.assertEqual(value['anchorReservation'],'conditional-on-sole-reaper')
            self.assertFalse(child.signal_group(int(signal.SIGKILL))['signalAuthority'])
        self.assertIsNone(child.receipt()['signalAuthority'])

    def test_permanent_capture_failure_is_not_cleanup_pass(self):
        class NeverAvailable:
            def observe(self,*args,**kwargs):
                raise p.AnchorError('anchor-bsd-unavailable')
        child=p.PrimaryAnchor.spawn([sys.executable,'-c','import sys;sys.stdin.read()'],
                             backend=NeverAvailable(),census=p.LinuxProcfsCensus(),stdin=subprocess.PIPE)
        self.children.append(child);CREATED.append(child._pid)
        with mock.patch.object(os,'killpg',side_effect=AssertionError('unbound-signal')):
            result=fixture_stop(child,time.monotonic()+.01)
        self.assertEqual(result['reason'],'anchor-drain-deadline')
        self.assertIsNone(result['exitCode'])
        self.assertIsNone(result['actualReaped'])
        self.assertEqual(result['cleanupStatus'],'unconfirmed')
        child.close_input()
        waited,raw=ORIGINAL_WAITPID(child._pid,0)
        self.assertEqual(waited,child._pid);EXTERNAL_REAPS[waited]=raw
        child._backend=p.LinuxWNOWAIT()
        self.assertEqual(child.observe()['reason'],'anchor-echild')

    def test_malformed_binding_types_never_authorize_signal(self):
        child=self.spawn()
        actual=child._backend.observe(child._pid,child._pid,child._binding)
        value=replace(actual,binding=replace(actual.binding,uid=False))
        with mock.patch.object(child._backend,'observe',return_value=value), \
                mock.patch.object(os,'killpg',side_effect=AssertionError('bad-type-signal')):
            result=child.signal_group(int(signal.SIGKILL))
        self.assertEqual((result['state'],result['reason'],result['signalAuthority']),
                         ('lost','anchor-observation-binding',False))

    def test_retained_finalizer_warns_but_never_polls(self):
        child=self.spawn()
        with mock.patch.object(child._process,'_internal_poll',side_effect=AssertionError('finalizer-reap')):
            with self.assertWarnsRegex(ResourceWarning,'^anchor-unresolved-quarantine$'):
                child._process.__del__()
        self.assertIsNone(child._process.returncode)
        self.assertEqual(child._state,'held')

    def test_reentrant_observer_cannot_reap_or_signal(self):
        child=self.spawn('pass');wait_zombie(child)
        actual=child._backend.observe
        nested=[]
        def reentrant(*args,**kwargs):
            nested.extend([child.try_reap(),child.signal_group(int(signal.SIGKILL)),
                           fixture_stop(child,time.monotonic()+1),child.observe()])
            return actual(*args,**kwargs)
        with mock.patch.object(child._backend,'observe',side_effect=reentrant), \
                mock.patch.object(os,'killpg') as kill:
            outer=child.signal_group(int(signal.SIGKILL))
        self.assertTrue(all(value['reason']=='anchor-operation-busy' for value in nested))
        self.assertTrue(all(value['signalAuthority'] is False for value in nested))
        self.assertEqual(kill.call_count,1)
        self.assertEqual(child._calls['waitpid'],0)
        self.assertEqual(outer['reason'],'anchor-signal-sent')
        self.assertEqual(child._state,'held')
        self.assertIsNone(child._process.returncode)

    def test_reentrant_last_observation_cannot_signal_during_final_reap(self):
        child=self.spawn('pass');wait_zombie(child)
        actual=child._backend.observe
        nested=[]
        def reentrant(*args,**kwargs):
            nested.append(child.signal_group(int(signal.SIGKILL)))
            return actual(*args,**kwargs)
        with mock.patch.object(child._backend,'observe',side_effect=reentrant), \
                mock.patch.object(os,'killpg',side_effect=AssertionError('reentrant-signal')):
            result=child.try_reap()
        self.assertEqual(result['status'],'reaped')
        self.assertEqual(len(nested),2)
        self.assertTrue(all(value['reason']=='anchor-operation-busy' for value in nested))

    def test_wrong_wait_pid_does_not_establish_reap(self):
        child = self.spawn('pass')
        wait_zombie(child)
        with mock.patch.object(os, 'waitpid', return_value=(child._pid+100000,0)):
            value = child.try_reap()
        self.assertEqual(value['reason'], 'anchor-wait-pid-mismatch')
        self.assertIsNone(value['actualReaped'])

    def test_capture_exception_returns_retained_recoverable_handle(self):
        backend = p.LinuxWNOWAIT()
        actual = backend.observe
        def first_bad(*args):
            backend.observe = actual
            raise ValueError('private-capture-sentinel')
        backend.observe = first_bad
        child = p.PrimaryAnchor.spawn([sys.executable,'-c','import sys;sys.stdin.read()'],
                               backend=backend,census=p.LinuxProcfsCensus(),stdin=subprocess.PIPE)
        self.children.append(child)
        CREATED.append(child._pid)
        self.assertIsNone(child._binding)
        self.assertEqual(child._state,'held')
        self.assertNotIn('private-capture-sentinel',json.dumps(child.receipt()))
        self.assertEqual(child.observe()['status'],'live')

    def test_constructor_interrupt_after_fork_retains_unknown_without_hidden_reap(self):
        # First os.read in this standard POSIX constructor is its exec-error pipe,
        # after fork and _child_created. Its interruption must not trigger poll/wait.
        with mock.patch.object(os,'read',side_effect=KeyboardInterrupt('private-interrupt')):
            with self.assertRaises(p.AnchorInterrupted) as caught:
                p.PrimaryAnchor.spawn([sys.executable,'-c','import sys;sys.stdin.read()'],
                    stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,
                    backend=p.LinuxWNOWAIT(),census=p.LinuxProcfsCensus())
        child=caught.exception.anchor
        self.assertIsNotNone(child)
        self.children.append(child);CREATED.append(child.pid)
        self.assertIsNone(child.actual_returncode)
        self.assertEqual(child.lifecycle_state,'lost')
        self.assertEqual(child.receipt()['reason'],'anchor-constructor-interrupted')
        with mock.patch.object(os,'waitpid',side_effect=AssertionError('unbound-recovery-wait')), \
                mock.patch.object(os,'killpg',side_effect=AssertionError('constructor-group-signal')):
            self.assertEqual(child.recover_exact_child()['reason'],'anchor-recovery-unbound')
            self.assertFalse(child.signal_group(int(signal.SIGKILL))['signalAuthority'])

    def test_actual_pid_assignment_before_child_flag_interrupt(self):
        constructor_gap_control(self,False)

    def test_actual_fork_return_before_pid_assignment_interrupt(self):
        constructor_gap_control(self,True)

    def test_exact_recovery_real_child_does_not_prove_group_cleanup(self):
        child=self.spawn('import sys;sys.exit(7)');wait_zombie(child)
        with mock.patch.object(os,'killpg',side_effect=AssertionError('recovery-signal')):
            result=child.recover_exact_child(deadline=time.monotonic()+1)
        self.assertEqual((result['status'],result['actualReturncode']),('recovered',7))
        self.assertIsNone(result['groupGone'])
        self.assertEqual(result['cleanupStatus'],'unconfirmed')

    def test_serialized_final_wait_blocks_competing_signal(self):
        child = self.spawn('pass')
        wait_zombie(child)
        entered, proceed, done = threading.Event(), threading.Event(), threading.Event()
        outputs = []
        def blocked(pid, flags):
            entered.set()
            self.assertTrue(proceed.wait(2))
            return ORIGINAL_WAITPID(pid, flags)
        def later_signal():
            outputs.append(child.signal_group(int(signal.SIGKILL)))
            done.set()
        with mock.patch.object(os,'waitpid',side_effect=blocked), \
                mock.patch.object(os,'killpg',side_effect=AssertionError('late-signal')):
            reap_thread = threading.Thread(target=lambda: outputs.append(child.try_reap()))
            reap_thread.start()
            self.assertTrue(entered.wait(2))
            signal_thread = threading.Thread(target=later_signal)
            signal_thread.start()
            self.assertFalse(done.wait(.02))
            proceed.set()
            reap_thread.join(2)
            signal_thread.join(2)
        self.assertFalse(reap_thread.is_alive() or signal_thread.is_alive())
        self.assertEqual({value['reason'] for value in outputs},{None,'anchor-authority-revoked'})

    def test_constructor_exec_missing_has_no_armed_child(self):
        count = len(p._registry)
        with self.assertRaises(p.AnchorSpawnError):
            p.PrimaryAnchor.spawn(['/definitely-not-an-executable-anchor-test'],
                           backend=p.LinuxWNOWAIT(),census=p.LinuxProcfsCensus())
        self.assertEqual(len(p._registry),count)

    def test_constructor_bad_cwd_has_no_armed_child(self):
        count = len(p._registry)
        with self.assertRaises(p.AnchorSpawnError):
            p.PrimaryAnchor.spawn([sys.executable,'-c','pass'],backend=p.LinuxWNOWAIT(),
                           census=p.LinuxProcfsCensus(),cwd='/definitely-not-an-anchor-directory')
        self.assertEqual(len(p._registry),count)

    def test_lifecycle_constructor_error_precedes_spawn(self):
        with mock.patch.object(p.PrimaryAnchor,'__init__',side_effect=ValueError('private')), \
                mock.patch.object(p,'GuardedPopen',side_effect=AssertionError('spawned')):
            with self.assertRaises(ValueError):
                p.PrimaryAnchor.spawn([sys.executable,'-c','pass'],backend=p.LinuxWNOWAIT(),census=p.LinuxProcfsCensus())

    def test_original_deadline_not_reset(self):
        child = self.spawn()
        before = dict(child._calls)
        self.assertEqual(fixture_stop(child,time.monotonic()-1)['reason'],'anchor-drain-deadline')
        self.assertEqual(dict(child._calls),before)
        self.assertIsNone(child._process.returncode)

    def test_strong_retention_and_capacity_fail_closed(self):
        child = self.spawn()
        self.assertIs(p._registry[child._pid],child)
        with mock.patch.object(p,'MAX_ANCHORS',len(p._registry)), \
                mock.patch.object(p,'GuardedPopen',side_effect=AssertionError('capacity-spawn')):
            with self.assertRaisesRegex(p.AnchorError,'^anchor-quarantine-limit$'):
                p.PrimaryAnchor.spawn([sys.executable,'-c','pass'],backend=p.LinuxWNOWAIT(),census=p.LinuxProcfsCensus())


    def test_linux_none_to_zombie_only_retries_same_bound_child(self):
        backend=p.LinuxWNOWAIT()
        row=('Z',os.getpid(),4321,123456)
        event=type('Wait',(),{'si_pid':4321,'si_uid':os.getuid(),'si_code':os.CLD_EXITED,'si_status':7})()
        uid=f'Uid:\t{os.getuid()}\t{os.geteuid()}\t{os.getuid()}\t{os.getuid()}\n'
        deadline=time.monotonic()+1
        with mock.patch.object(os,'waitid',side_effect=[None,event]) as call, \
                mock.patch.object(p,'_linux_stat',return_value=row), \
                mock.patch.object(p,'_read_small',return_value=uid):
            result=backend.observe(4321,4321,deadline=deadline)
        self.assertEqual((result.state,p.decode_status(result.raw_status)),('zombie',7))
        self.assertEqual(call.call_count,2)
        for args in call.call_args_list:
            self.assertEqual(args.args,(os.P_PID,4321,os.WEXITED|os.WNOWAIT|os.WNOHANG))

    def test_linux_none_zombie_second_unknown_stays_unknown(self):
        backend=p.LinuxWNOWAIT()
        row=('Z',os.getpid(),4321,123456)
        uid=f'Uid:\t{os.getuid()}\t{os.geteuid()}\t0\t0\n'
        with mock.patch.object(os,'waitid',return_value=None) as call, \
                mock.patch.object(p,'_linux_stat',return_value=row), \
                mock.patch.object(p,'_read_small',return_value=uid):
            with self.assertRaisesRegex(p.AnchorError,'^anchor-terminal-race$'):
                backend.observe(4321,4321,deadline=time.monotonic()+1)
        self.assertEqual(call.call_count,2)

    def test_linux_race_retry_binding_change_is_not_fallback(self):
        backend=p.LinuxWNOWAIT()
        first=('Z',os.getpid(),4321,123456)
        changed=('Z',os.getpid(),4321,123457)
        uid=f'Uid:\t{os.getuid()}\t{os.geteuid()}\t0\t0\n'
        with mock.patch.object(os,'waitid',return_value=None), \
                mock.patch.object(p,'_linux_stat',side_effect=[first,first,changed,changed]), \
                mock.patch.object(p,'_read_small',return_value=uid):
            with self.assertRaisesRegex(p.AnchorError,'^anchor-generation-changed$'):
                backend.observe(4321,4321,deadline=time.monotonic()+1)


class ModelTests(unittest.TestCase):
    def test_rlock_alone_model_allows_stale_signal_after_reentrant_reap(self):
        lock=threading.RLock();state={'held':True,'signaled':False}
        with lock:
            with lock:
                state['held']=False
            # Model of unchecked outer signal continuation; no OS calls.
            state['signaled']=True
        self.assertEqual(state,{'held':False,'signaled':True})
    def bsd(self, **changes):
        value = darwin.BSDInfo()
        for name,item in {'pid':4321,'ppid':os.getpid(),'uid':os.geteuid(),'ruid':os.getuid(),
                          'pgid':4321,'start_sec':123456,'start_usec':789,'status':5,'xstatus':1792,
                          **changes}.items():
            setattr(value,name,item)
        calls=[]
        def query(pid, flavor, output):
            calls.append((pid,flavor,ctypes.sizeof(output)))
            ctypes.memmove(ctypes.byref(output),ctypes.byref(value),ctypes.sizeof(value))
            return ctypes.sizeof(value),0
        reader=type('Reader',(),{'query':staticmethod(query)})()
        adapter=p.DarwinBSD(reader,darwin.BSDInfo,os.getpid(),os.geteuid(),os.getuid())
        return adapter,reader,calls

    def test_darwin_full_bsd_low16_and_no_resource_zero(self):
        adapter,_,calls=self.bsd(xstatus=0xdead0700)
        value=adapter.observe(4321,4321)
        self.assertEqual(value.state,'zombie')
        self.assertEqual(p.decode_status(value.raw_status),7)
        self.assertEqual(calls,[(4321,3,136)])

    def test_actual_query_adapter_keeps_bsd_arg1(self):
        calls=[]
        class Lib:
            @staticmethod
            def proc_pidinfo(pid,flavor,arg,output,size):
                calls.append((pid,flavor,arg,size))
                return 0
        reader=darwin.DarwinActivity.__new__(darwin.DarwinActivity)
        reader.lib=Lib()
        adapter=p.DarwinBSD(reader,darwin.BSDInfo,os.getpid(),os.geteuid(),os.getuid())
        with self.assertRaisesRegex(p.AnchorError,'^anchor-bsd-unavailable$'):
            adapter.observe(4321,4321)
        self.assertEqual(calls,[(4321,3,1,136)])

    def test_darwin_identity_fields_and_states(self):
        changes=[{'pid':4322},{'ppid':1},{'uid':os.geteuid()+1},{'ruid':os.getuid()+1},
                 {'pgid':4322},{'start_sec':0},{'start_usec':1000000},{'status':0},{'status':6}]
        for change in changes:
            with self.subTest(change=change):
                adapter,_,_=self.bsd(**change)
                with self.assertRaisesRegex(p.AnchorError,'^anchor-bsd-binding$'):
                    adapter.observe(4321,4321)

    def test_darwin_short_errno_and_bad_generation(self):
        for size,error in [(0,0),(135,0),(137,0),(136,errno.ESRCH),(True,0),(136,False)]:
            adapter,reader,_=self.bsd()
            reader.query=lambda *args:(size,error)
            with self.subTest(size=size,error=error):
                with self.assertRaisesRegex(p.AnchorError,'^anchor-bsd-unavailable$'):
                    adapter.observe(4321,4321)
        adapter,_,_=self.bsd()
        original=adapter.observe(4321,4321).binding
        with self.assertRaisesRegex(p.AnchorError,'^anchor-generation-changed$'):
            adapter.observe(4321,4321,replace(original,birth=(123457,789)))

    def test_darwin_live_never_decodes_stale_xstatus(self):
        adapter,_,_=self.bsd(status=2,xstatus=0xffffffff)
        value=adapter.observe(4321,4321)
        self.assertEqual((value.state,value.raw_status),('live',None))

    def test_darwin_undefined_signal_rejected(self):
        for sig in (32,42,126):
            adapter,_,_=self.bsd(xstatus=sig)
            with self.subTest(sig=sig),self.assertRaisesRegex(p.AnchorError,'^anchor-wait-signal$'):
                adapter.observe(4321,4321)




    def test_wait_status_models(self):
        for code in (0,7,255):
            self.assertEqual(p.decode_status(code<<8),code)
        for sig in (2,9,15):
            self.assertEqual(p.decode_status(sig),-sig)
        self.assertEqual(p.decode_status(6|128),-6)
        for bad in (True,None,-1,65536,0xffff,0x137f,0x0709):
            with self.subTest(bad=bad),self.assertRaises(p.AnchorError):
                p.decode_status(bad)

    def test_raw_roster_strict_types_unknown_not_empty(self):
        valid={'status':'complete','group':4321,'pids':[4321,4322],
               'source':('darwin-header-bound' if sys.platform=='darwin' else 'linux-procfs-test'),'bindingVerified':True}
        self.assertEqual(p.raw_roster(valid,4321),{4321,4322})
        variants=[None,{},dict(valid,status='unavailable'),dict(valid,pids=None),
                  dict(valid,pids=[4321,True]),dict(valid,pids=[4321,4321]),
                  dict(valid,group=True),dict(valid,bindingVerified=1),
                  dict(valid,source='registry'),dict(valid,privatePath='sentinel')]
        for value in variants:
            with self.subTest(value=value),self.assertRaises(p.AnchorError):
                p.raw_roster(value,4321)

    def test_unknown_post_reap_read_only(self):
        anchor=p.PrimaryAnchor.__new__(p.PrimaryAnchor)
        anchor._lock=threading.RLock();anchor._state='lost';anchor._pid=4321
        anchor._bootstrap_stage=None
        anchor._census=lambda group,**kwargs:None
        with mock.patch.object(os,'killpg',side_effect=AssertionError('read-signal')):
            self.assertEqual(anchor.post_reap_absence(),{'status':'unconfirmed',
                'groupEmpty':None,'readOnly':True,'signalAuthority':False,
                'errorType':'anchor-error','errno':None})


class ProductionModelTests(unittest.TestCase):
    def setUp(self):
        self.registry=dict(p._registry)
        self.opaque_registry=dict(p._opaque_registry)

    def tearDown(self):
        p._registry.clear();p._registry.update(self.registry)
        p._opaque_registry.clear();p._opaque_registry.update(self.opaque_registry)

    def fake(self, terminal=True):
        class Process:
            pid=4321;returncode=None;_child_created=True
            stdin=stdout=stderr=None
            def arm(self):self._armed=True
        class Backend:
            def observe(self,pid,pgid,expected=None,**kwargs):
                binding=p.Binding(pid,os.getpid(),os.geteuid(),os.getuid(),pgid,(123456,789))
                return p.Observation(binding,'zombie' if terminal else 'live',0 if terminal else None)
        anchor=p.PrimaryAnchor(Process(),Backend(),None)
        anchor.observe()
        return anchor

    def test_spawn_forwards_stdio_env_and_fixes_new_session(self):
        calls=[];fake=self.fake(False)
        process=fake.process
        def initialize(*args,**kwargs):calls.append((args,kwargs))
        process.__init__=initialize
        env={'SINGLE_FIXED':'value'}
        with mock.patch.object(p.GuardedPopen,'__new__',return_value=process):
            anchor=p.PrimaryAnchor.spawn(['fixed-executable'],cwd='fixed-cwd',env=env,
                stdin=None,stdout=11,stderr=12,backend=fake._backend)
        self.assertEqual(calls,[( (['fixed-executable'],), {'cwd':'fixed-cwd','env':env,
            'stdin':None,'stdout':11,'stderr':12,'start_new_session':True})])
        self.assertEqual((anchor.pid,anchor.actual_returncode,anchor.observed_exit_code,anchor.held),
                         (4321,None,None,True))
        with self.assertRaises(AttributeError):anchor.pid=123

    def test_no_production_session_override_or_stop(self):
        with self.assertRaises(TypeError):p.PrimaryAnchor.spawn(['fixed'],new_session=False)
        self.assertFalse(hasattr(p.PrimaryAnchor,'stop'))

    def test_sigchld_ignore_refuses_before_spawn(self):
        with mock.patch.object(signal,'getsignal',return_value=signal.SIG_IGN), \
                mock.patch.object(p.GuardedPopen,'__new__',side_effect=AssertionError('spawned')):
            with self.assertRaisesRegex(p.AnchorError,'^anchor-sigchld-not-default$'):
                p.PrimaryAnchor.spawn(['fixed'])

    def test_sigchld_handler_change_revokes_authority(self):
        anchor=self.fake()
        with mock.patch.object(signal,'getsignal',return_value=lambda *_:None), \
                mock.patch.object(os,'killpg',side_effect=AssertionError('handler-signal')):
            result=anchor.signal_group(int(signal.SIGKILL))
        self.assertEqual((result['state'],result['reason'],result['signalAuthority']),
                         ('lost','anchor-sigchld-not-default',False))

    def test_constructor_interruption_retains_actual_instance_unknown(self):
        process=self.fake().process
        def initialize(*args,**kwargs):raise KeyboardInterrupt('private-interrupt')
        process.__init__=initialize
        with mock.patch.object(p.GuardedPopen,'__new__',return_value=process):
            with self.assertRaises(p.AnchorInterrupted) as caught:
                p.PrimaryAnchor.spawn(['fixed'],backend=self.fake()._backend)
        anchor=caught.exception.anchor
        self.assertIs(anchor.process,process)
        self.assertEqual((anchor.lifecycle_state,anchor.actual_returncode),('lost',None))
        self.assertIs(p._registry[4321],anchor)
        with mock.patch.object(os,'waitpid',side_effect=AssertionError('unbound-reap')):
            self.assertEqual(anchor.recover_exact_child()['reason'],'anchor-recovery-unbound')

    def test_constructor_inner_failure_without_pid_is_opaque_unknown(self):
        process=self.fake().process;process._child_created=False;process.pid=None
        process.__init__=lambda *args,**kwargs:(_ for _ in ()).throw(ValueError('private'))
        with mock.patch.object(p.GuardedPopen,'__new__',return_value=process):
            with self.assertRaises(p.AnchorSpawnError) as caught:
                p.PrimaryAnchor.spawn(['fixed'],backend=self.fake()._backend)
        anchor=caught.exception.anchor
        self.assertIsNotNone(anchor)
        self.assertIsNone(anchor.pid);self.assertIsNone(anchor.actual_returncode)
        self.assertIs(p._opaque_registry[id(process)],anchor)
        self.assertNotIn(4321,p._registry)

    def test_known_preconstructor_validation_failure_allocates_nothing(self):
        before=p.quarantine_receipt()
        with mock.patch.object(p.GuardedPopen,'__new__',side_effect=AssertionError('allocated')):
            with self.assertRaisesRegex(p.AnchorError,'^anchor-argv$'):
                p.PrimaryAnchor.spawn([])
        self.assertEqual(p.quarantine_receipt(),before)

    def test_positive_pid_with_false_child_flag_is_retained(self):
        process=self.fake().process;process._child_created=False
        process.__init__=lambda *args,**kwargs:(_ for _ in ()).throw(KeyboardInterrupt('private'))
        with mock.patch.object(p.GuardedPopen,'__new__',return_value=process):
            with self.assertRaises(p.AnchorInterrupted) as caught:
                p.PrimaryAnchor.spawn(['fixed'],backend=self.fake()._backend)
        anchor=caught.exception.anchor
        self.assertIs(p._registry[4321],anchor);self.assertTrue(process._armed)
        self.assertFalse(process._child_created);self.assertIsNone(anchor.actual_returncode)
        self.assertIs(anchor.receipt()['signalAuthority'],False)

    def test_success_then_arm_interruption_retains_pending_owner(self):
        process=self.fake().process;process.__init__=lambda *args,**kwargs:None
        calls=[]
        def arm():
            calls.append(1)
            if len(calls)==1:raise KeyboardInterrupt('fixed-after-constructor')
            process._armed=True
        process.arm=arm
        with mock.patch.object(p.GuardedPopen,'__new__',return_value=process):
            with self.assertRaises(p.AnchorInterrupted) as caught:
                p.PrimaryAnchor.spawn(['fixed'],backend=self.fake()._backend)
        self.assertIs(p._registry[4321],caught.exception.anchor)
        self.assertEqual(len(calls),2)

    def test_known_registry_registration_interruption_keeps_pending_owner(self):
        class OnceInterrupted(dict):
            interrupted=False
            def __setitem__(self,key,value):
                if not self.interrupted:
                    self.interrupted=True;raise KeyboardInterrupt('fixed-registration-boundary')
                return super().__setitem__(key,value)
        process=self.fake().process;process.__init__=lambda *args,**kwargs:None
        registry=OnceInterrupted()
        with mock.patch.object(p,'_registry',registry), \
                mock.patch.object(p.GuardedPopen,'__new__',return_value=process):
            with self.assertRaises(p.AnchorInterrupted) as caught:
                p.PrimaryAnchor.spawn(['fixed'],backend=self.fake()._backend)
            self.assertIs(registry[4321],caught.exception.anchor)
        self.assertNotIn(id(process),p._opaque_registry)

    def test_unknown_pid_blocks_all_wait_signal_and_absence_queries(self):
        anchor=self.fake();anchor._pid=None;anchor._binding=None;anchor._state='lost'
        callback=mock.Mock(side_effect=AssertionError('unknown-census'))
        with mock.patch.object(os,'waitpid',side_effect=AssertionError('unknown-wait')), \
                mock.patch.object(os,'killpg',side_effect=AssertionError('unknown-signal')):
            self.assertEqual(anchor.recover_exact_child()['reason'],'anchor-recovery-unbound')
            self.assertFalse(anchor.signal_group(int(signal.SIGKILL))['signalAuthority'])
            self.assertIsNone(anchor.post_reap_absence(callback)['groupEmpty'])
        callback.assert_not_called()
        self.assertFalse(anchor.receipt()['pidKnown'])
        self.assertEqual(anchor.receipt()['childExistence'],'unconfirmed')

    def test_opaque_quarantine_capacity_is_bounded_before_spawn(self):
        anchor=self.fake();anchor._pid=None;anchor._state='lost'
        p._opaque_registry[id(anchor.process)]=anchor
        with mock.patch.object(p,'MAX_ANCHORS',len(p.retained_anchors())), \
                mock.patch.object(p.GuardedPopen,'__new__',side_effect=AssertionError('capacity-spawn')):
            with self.assertRaisesRegex(p.AnchorError,'^anchor-quarantine-limit$'):
                p.PrimaryAnchor.spawn(['fixed'],backend=anchor._backend)
        self.assertEqual(p.quarantine_receipt()['opaque'],1)

    def test_started_unknown_object_finalizer_warns_without_base_poll(self):
        process=p.GuardedPopen.__new__(p.GuardedPopen)
        process._constructor_started=True;process._child_created=False
        with mock.patch.object(subprocess.Popen,'__del__',side_effect=AssertionError('base-finalizer')), \
                mock.patch.object(os,'waitpid',side_effect=AssertionError('finalizer-wait')):
            with self.assertWarnsRegex(ResourceWarning,'^anchor-unresolved-quarantine$'):
                process.__del__()
            with self.assertRaisesRegex(p.AnchorError,'^anchor-hidden-reap-forbidden$'):
                process.poll()

    def test_positive_exact_reap_fact_prevents_second_recovery_wait(self):
        anchor=self.fake();anchor._state='lost';anchor._reason='anchor-wait-status-mismatch'
        anchor._actual_reaped=True;anchor.process.returncode=7
        before=anchor.receipt()
        with mock.patch.object(os,'waitpid',side_effect=AssertionError('second-wait')), \
                mock.patch.object(os,'killpg',side_effect=AssertionError('recovery-signal')):
            self.assertEqual(anchor.recover_exact_child(),before)
        self.assertEqual(anchor.actual_returncode,7)

    def test_constructor_explicit_failed_exec_known_returncode_separate(self):
        process=self.fake().process;process.returncode=127
        process.__init__=lambda *args,**kwargs:(_ for _ in ()).throw(FileNotFoundError('private'))
        with mock.patch.object(p.GuardedPopen,'__new__',return_value=process):
            with self.assertRaises(p.AnchorSpawnError) as caught:
                p.PrimaryAnchor.spawn(['fixed'],backend=self.fake()._backend)
        self.assertEqual(caught.exception.anchor.actual_returncode,127)
        self.assertIs(caught.exception.anchor.receipt()['actualReaped'],True)
        self.assertNotIn(4321,p._registry)

    def test_callback_deadline_exact_and_no_typeerror_fallback(self):
        anchor=self.fake();deadline=time.monotonic()+1;calls=[]
        def callback(group,**kwargs):
            calls.append((group,kwargs));raise TypeError('private-callback-error')
        with mock.patch.object(os,'waitpid',side_effect=AssertionError('callback-reap')):
            result=anchor.try_reap(callback,deadline=deadline)
        self.assertEqual(calls,[(4321,{'deadline':deadline})])
        self.assertEqual(result['reason'],'anchor-observation-unavailable')
        self.assertIs(result['actualReturncode'],None)

    def test_postreap_deadline_exact_and_absence_typed(self):
        anchor=self.fake();anchor._state='lost';calls=[];deadline=time.monotonic()+1
        def callback(group,**kwargs):
            calls.append((group,kwargs));return {'status':'complete','group':group,'pids':[],
                'source':'darwin-header-bound' if sys.platform=='darwin' else 'linux-ps-test',
                'bindingVerified':True}
        result=anchor.post_reap_absence(callback,deadline=deadline)
        self.assertEqual(calls,[(4321,{'deadline':deadline})])
        self.assertEqual(result,{'status':'observed','groupEmpty':True,'readOnly':True,'signalAuthority':False,'errorType':None,'errno':None})

    def test_darwin_bootstrap_source_only_two_named_stages(self):
        receipt={'status':'complete','group':4321,'pids':[4321],
                 'source':'darwin-bootstrap-ps','bindingVerified':True}
        with mock.patch.object(sys,'platform','darwin'):
            for stage in ('census-abi-compile','census-abi'):
                self.assertEqual(p.raw_roster(receipt,4321,stage),{4321})
            for stage in (None,'full-build','other'):
                with self.assertRaises(p.AnchorError):p.raw_roster(receipt,4321,stage)
        with mock.patch.object(sys,'platform','linux'):
            with self.assertRaises(p.AnchorError):p.raw_roster(receipt,4321,'census-abi')

    def test_linux_test_census_cannot_qualify_darwin(self):
        for source in ('linux-procfs-test','linux-ps-test'):
            receipt={'status':'complete','group':4321,'pids':[4321],'source':source,'bindingVerified':True}
            with mock.patch.object(sys,'platform','darwin'):
                with self.assertRaises(p.AnchorError):p.raw_roster(receipt,4321)

    def test_recovery_wait_nonblocking_without_signals_or_cleanup_qualification(self):
        anchor=self.fake();anchor._state='lost';anchor._reason='anchor-observation-unavailable'
        with mock.patch.object(os,'waitpid',return_value=(4321,0)) as wait, \
                mock.patch.object(os,'killpg',side_effect=AssertionError('recovery-signal')):
            result=anchor.recover_exact_child(deadline=time.monotonic()+1)
        wait.assert_called_once_with(4321,os.WNOHANG)
        self.assertEqual((result['status'],result['actualReturncode'],result['groupGone']),
                         ('recovered',0,None))
        self.assertIs(result['signalAuthority'],False)
        self.assertEqual(result['cleanupStatus'],'unconfirmed')

    def test_recovery_after_echild_never_waits_cached_pid(self):
        anchor=self.fake();anchor._state='lost';anchor._reason='anchor-echild'
        with mock.patch.object(os,'waitpid',side_effect=AssertionError('reused-child-wait')):
            result=anchor.recover_exact_child()
        self.assertEqual(result['reason'],'anchor-echild')
        self.assertIsNone(result['actualReturncode'])

    def test_recovery_zero_never_sets_returncode(self):
        anchor=self.fake()
        with mock.patch.object(os,'waitpid',return_value=(0,0)):
            result=anchor.recover_exact_child()
        self.assertEqual(result['reason'],'anchor-recovery-not-ready')
        self.assertIsNone(anchor.actual_returncode)
        self.assertIs(result['signalAuthority'],False)

    def test_recovery_generation_mismatch_never_waits(self):
        anchor=self.fake();original=anchor._binding
        anchor._backend.observe=lambda *args,**kwargs:p.Observation(
            replace(original,birth=(123457,789)),'zombie',0)
        with mock.patch.object(os,'waitpid',side_effect=AssertionError('generation-wait')):
            result=anchor.recover_exact_child()
        self.assertEqual(result['reason'],'anchor-generation-changed')

    def test_failed_final_wait_can_recover_only_still_bound_exact_child(self):
        anchor=self.fake();anchor._state='lost';anchor._reason='anchor-final-wait-unavailable'
        with mock.patch.object(os,'waitpid',return_value=(4321,1792)):
            result=anchor.recover_exact_child()
        self.assertEqual(result['reason'],'anchor-wait-status-mismatch')
        self.assertEqual(anchor.actual_returncode,7)
        self.assertEqual(result['state'],'lost')

    def test_unmeasured_terminal_resource_stays_none(self):
        anchor=self.fake()
        self.assertEqual(anchor.observe()['status'],'zombie')
        self.assertEqual(anchor.receipt()['resources'],{'rssMiB':None,'cpuSeconds':None})

    def test_recovery_callback_reentry_no_extra_os_operation(self):
        anchor=self.fake();backend=anchor._backend.observe;nested=[]
        def callback(*args,**kwargs):
            nested.append(anchor.recover_exact_child())
            nested.append(anchor.signal_group(int(signal.SIGKILL)))
            return backend(*args,**kwargs)
        anchor._backend.observe=callback
        with mock.patch.object(os,'waitpid',return_value=(4321,0)) as wait, \
                mock.patch.object(os,'killpg',side_effect=AssertionError('reentry')):
            self.assertEqual(anchor.recover_exact_child()['status'],'recovered')
        self.assertTrue(all(value['reason']=='anchor-operation-busy' for value in nested))
        self.assertEqual(wait.call_count,1)

    def test_safe_errno_and_stale_positive_exit_clear_on_unavailable(self):
        anchor=self.fake();self.assertEqual(anchor.observed_exit_code,0)
        backend=anchor._backend.observe
        anchor._backend.observe=lambda *args,**kwargs:(_ for _ in ()).throw(
            PermissionError(errno.EACCES,'private-errno-sentinel'))
        value=anchor.observe()
        self.assertEqual((value['errorType'],value['errno'],value['observedExitCode']),
                         ('os-error',errno.EACCES,None))
        self.assertIs(value['signalAuthority'],False)
        self.assertNotIn('private-errno-sentinel',json.dumps(value))
        anchor._backend.observe=backend
        value=anchor.observe()
        self.assertEqual((value['errorType'],value['errno'],value['observedExitCode']),
                         (None,None,0))

    def test_signal_errno_safe_and_success_clears_it(self):
        anchor=self.fake(False)
        with mock.patch.object(os,'killpg',side_effect=PermissionError(errno.EPERM,'private-signal')):
            value=anchor.signal_group(int(signal.SIGTERM))
        self.assertEqual((value['errorType'],value['errno']),('os-error',errno.EPERM))
        self.assertNotIn('private-signal',json.dumps(value))
        with mock.patch.object(os,'killpg'):
            value=anchor.signal_group(int(signal.SIGTERM))
        self.assertEqual((value['errorType'],value['errno']),(None,None))

    def test_bool_errno_is_not_an_integer_measurement(self):
        anchor=self.fake();error=OSError('private');error.errno=True
        anchor._backend.observe=lambda *args,**kwargs:(_ for _ in ()).throw(error)
        value=anchor.observe()
        self.assertEqual(value['errorType'],'os-error');self.assertIsNone(value['errno'])


@unittest.skipUnless(sys.platform=='darwin','Darwin actual-child tests')
class DarwinActualTests(unittest.TestCase):
    """Select explicitly after the existing header ABI bootstrap; zero skips on Mac."""
    def setUp(self):
        self.assertIn('FLOORP_CENSUS_ABI',os.environ,'anchor-mac-abi-required')
        self.children=[]

    def census(self,group,*,deadline=None):
        from darwin_inventory import collect
        p.check_deadline(deadline)
        value=collect(group,[])
        p.check_deadline(deadline)
        return {'status':'complete','group':group,'pids':value['pids'],
                'source':'darwin-header-bound','bindingVerified':True}

    def spawn(self,code='import sys;sys.stdin.buffer.read();sys.exit(7)',**kwargs):
        kwargs.setdefault('stdin',subprocess.PIPE)
        kwargs.setdefault('stdout',subprocess.DEVNULL);kwargs.setdefault('stderr',subprocess.DEVNULL)
        child=p.PrimaryAnchor.spawn([sys.executable,'-c',code],census=self.census,**kwargs)
        self.children.append(child);CREATED.append(child.pid)
        return child

    def tearDown(self):
        for child in self.children:
            if child.held:
                self.assertEqual(fixture_stop(child,time.monotonic()+3)['status'],'reaped')
            if child.actual_returncode is None:
                with self.assertWarns(ResourceWarning):child.process.warn_unresolved()
                if child.pid in EXTERNAL_REAPS:
                    raw=EXTERNAL_REAPS.pop(child.pid)
                else:
                    # Fixture-only direct child, no cached group signal.
                    os.kill(child.pid,signal.SIGKILL)
                    waited,raw=ORIGINAL_WAITPID(child.pid,0);self.assertEqual(waited,child.pid)
                child.process.returncode=os.waitstatus_to_exitcode(raw)
            for stream in (child.process.stdin,child.process.stdout,child.process.stderr):
                if stream is not None and not stream.closed:stream.close()
            p._registry.pop(child.pid,None)

    def check_exit(self,code):
        child=self.spawn(f'import sys;sys.exit({code})');observed=wait_zombie(child)
        self.assertEqual(observed['observedExitCode'],code)
        self.assertIsNone(child.actual_returncode)
        result=child.try_reap(deadline=time.monotonic()+3)
        self.assertEqual((result['status'],result['actualReturncode']),('reaped',code))

    def test_01_exit_zero(self):self.check_exit(0)
    def test_02_exit_seven(self):self.check_exit(7)
    def test_03_exit_255(self):self.check_exit(255)

    def check_signal(self,sig):
        child=self.spawn();self.assertEqual(child.signal_group(int(sig))['reason'],'anchor-signal-sent')
        self.assertEqual(wait_zombie(child)['observedExitCode'],-sig)
        self.assertEqual(child.try_reap(deadline=time.monotonic()+3)['actualReturncode'],-sig)

    def test_04_signal_term(self):self.check_signal(signal.SIGTERM)
    def test_05_signal_int(self):self.check_signal(signal.SIGINT)
    def test_06_signal_kill(self):self.check_signal(signal.SIGKILL)

    def test_07_zombie_observation_never_reaps(self):
        child=self.spawn('pass');wait_zombie(child)
        for _ in range(3):self.assertEqual(child.observe()['status'],'zombie')
        self.assertIsNone(child.actual_returncode)

    def test_08_default_stdin_is_none(self):
        child=self.spawn('pass',stdin=None);self.assertIsNone(child.process.stdin);wait_zombie(child)

    def test_09_cwd_env_output_forwarding(self):
        import tempfile
        with tempfile.TemporaryDirectory() as temp:
            output=Path(temp)/'fixed.out'
            with output.open('wb') as stream:
                child=self.spawn('import os;print(os.environ["ANCHOR_FIXED"],flush=True)',
                                 cwd=temp,env={**os.environ,'ANCHOR_FIXED':'fixed-value'},stdout=stream)
                wait_zombie(child);self.assertEqual(child.try_reap()['status'],'reaped')
            self.assertEqual(output.read_text(),'fixed-value\n')

    def test_10_native_census_preserves_zombie(self):
        child=self.spawn('pass');wait_zombie(child)
        self.assertIn(child.pid,self.census(child.pid)['pids'])

    def test_11_duplicate_finalization_does_not_wait_or_signal(self):
        child=self.spawn('pass');wait_zombie(child);self.assertEqual(child.try_reap()['status'],'reaped')
        with mock.patch.object(os,'waitpid',side_effect=AssertionError('duplicate-wait')), \
                mock.patch.object(os,'killpg',side_effect=AssertionError('duplicate-signal')):
            self.assertEqual(child.try_reap()['status'],'reaped')
            self.assertFalse(child.signal_group(int(signal.SIGKILL))['signalAuthority'])

    def test_12_post_reap_empty_is_read_only(self):
        child=self.spawn('pass');wait_zombie(child);child.try_reap()
        with mock.patch.object(os,'killpg',side_effect=AssertionError('absence-signal')):
            self.assertTrue(child.post_reap_absence(deadline=time.monotonic()+3)['groupEmpty'])

    def test_13_unknown_census_is_not_empty(self):
        child=self.spawn('pass');wait_zombie(child)
        self.assertEqual(child.try_reap(lambda *args,**kwargs:None)['reason'],'anchor-census-schema')
        self.assertIsNone(child.actual_returncode)

    def test_14_unavailable_bsd_never_uses_cached_authority(self):
        child=self.spawn()
        with mock.patch.object(child._backend.reader,'query',return_value=(0,errno.ESRCH)), \
                mock.patch.object(os,'killpg',side_effect=AssertionError('unavailable-signal')):
            value=child.signal_group(int(signal.SIGKILL))
        self.assertFalse(value['signalAuthority']);self.assertIsNone(child.actual_returncode)

    def test_15_fast_live_to_terminal_remains_same_birth(self):
        child=self.spawn();birth=child._binding;child.close_input();wait_zombie(child)
        self.assertEqual(child._binding,birth)

    def test_16_failed_exec_standard_constructor_reap(self):
        with self.assertRaises(p.AnchorSpawnError) as caught:
            p.PrimaryAnchor.spawn(['/definitely-missing-anchor-executable'])
        anchor=caught.exception.anchor
        self.assertIsNotNone(anchor);self.assertTrue(anchor.receipt()['actualReaped'])
        self.assertIsNotNone(anchor.actual_returncode)

    def test_17_external_reap_stays_unknown(self):
        child=self.spawn('pass');wait_zombie(child)
        waited,raw=ORIGINAL_WAITPID(child.pid,0);self.assertEqual(waited,child.pid)
        EXTERNAL_REAPS[child.pid]=raw
        result=child.observe()
        self.assertEqual(result['status'],'unavailable');self.assertIsNone(child.actual_returncode)
        # No cached-PGID signal is attempted while native identity is unavailable.
        with mock.patch.object(os,'killpg',side_effect=AssertionError('external-reap-signal')):
            self.assertFalse(child.signal_group(int(signal.SIGKILL))['signalAuthority'])
        child._state='lost';child._reason='anchor-echild'

    def test_18_bsd_low16_equals_actual_exact_wait_status(self):
        child=self.spawn('import sys;sys.exit(7)');wait_zombie(child)
        observation=child._backend.observe(child.pid,child.pid,child._binding)
        captured=[]
        def actual_wait(pid,flags):
            value=ORIGINAL_WAITPID(pid,flags);captured.append(value);return value
        with mock.patch.object(os,'waitpid',side_effect=actual_wait):
            result=child.try_reap(deadline=time.monotonic()+3)
        self.assertEqual(captured,[(child.pid,observation.raw_status)])
        self.assertEqual(result['actualReturncode'],7)

    def test_19_actual_pid_assignment_before_child_flag_interrupt(self):
        constructor_gap_control(self,False)

    def test_20_actual_fork_return_before_pid_assignment_interrupt(self):
        constructor_gap_control(self,True)


if __name__=='__main__':
    unittest.main()

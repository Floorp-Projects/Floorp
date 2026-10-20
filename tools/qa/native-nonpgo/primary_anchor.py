# SPDX-License-Identifier: MPL-2.0
"""Retain an exact direct child until a serialized, positive final wait.

Census callbacks are trusted, source-bound interfaces, never PID registry authority.
This helper supplies no resource measurement, cleanup timing policy or session closure.
Actual Darwin qualification is separate from Linux/model tests.
"""
from collections import Counter
from dataclasses import dataclass
import errno
from functools import wraps
import math
import os
from pathlib import Path
import signal
import subprocess
import sys
import threading
import time
import warnings

MAX_ANCHORS = 8
MAX_ROSTER = 32768
_registry = {}
_opaque_registry = {}
_registry_lock = threading.RLock()
METRICS = Counter()
FIXED_CODES = frozenset({
    'anchor-argv', 'anchor-bootstrap-stage', 'anchor-bsd-binding', 'anchor-bsd-unavailable', 'anchor-census-limit',
    'anchor-census-pids', 'anchor-census-schema', 'anchor-census-source', 'anchor-census-unavailable',
    'anchor-constructor-failed', 'anchor-constructor-interrupted',
    'anchor-deadline', 'anchor-echild', 'anchor-final-wait-interrupted',
    'anchor-final-wait-unavailable', 'anchor-generation-changed',
    'anchor-hidden-reap-forbidden', 'anchor-missing-from-roster',
    'anchor-observation-binding', 'anchor-observation-schema',
    'anchor-observation-unavailable', 'anchor-operation-deadline', 'anchor-operation-busy',
    'anchor-popen-status-mutated', 'anchor-proc-binding', 'anchor-proc-schema',
    'anchor-proc-size', 'anchor-quarantine-limit', 'anchor-session', 'anchor-signal',
    'anchor-platform', 'anchor-sigchld-not-default', 'anchor-recovery-unbound',
    'anchor-recovery-not-terminal', 'anchor-recovery-not-ready',
    'anchor-terminal-mismatch', 'anchor-terminal-race', 'anchor-wait-pid-mismatch',
    'anchor-wait-signal', 'anchor-wait-status', 'anchor-wait-status-mismatch',
    'anchor-wait-not-ready', 'anchor-waitid-binding'})


class AnchorError(RuntimeError):
    pass


class AnchorSpawnError(AnchorError):
    def __init__(self, code, anchor=None):
        super().__init__(code)
        self.anchor = anchor


class AnchorInterrupted(KeyboardInterrupt):
    def __init__(self, code, anchor=None):
        super().__init__(code)
        self.anchor = anchor


def sole_reaper_check():
    require(signal.getsignal(signal.SIGCHLD) == signal.SIG_DFL,
            'anchor-sigchld-not-default')


def require(condition, code):
    if not condition:
        raise AnchorError(code)


def integer(value, minimum=0, maximum=2**31 - 1):
    return type(value) is int and minimum <= value <= maximum


def valid_exit_code(value):
    return type(value) is int and (0 <= value <= 255 or -value in signal.valid_signals())


def retained_anchors():
    # Pending-to-known handoff may temporarily hold the same owner in both maps.
    return tuple({id(anchor):anchor for anchor in
                  (*_registry.values(),*_opaque_registry.values())}.values())


def decode_status(raw, valid_signals=None):
    require(integer(raw, 0, 65535), 'anchor-wait-status')
    require((os.WIFEXITED(raw) and raw & 255 == 0) or
            (os.WIFSIGNALED(raw) and raw >> 8 == 0), 'anchor-wait-status')
    decoded = os.waitstatus_to_exitcode(raw)
    valid_signals = signal.valid_signals() if valid_signals is None else valid_signals
    require(decoded >= 0 or -decoded in valid_signals, 'anchor-wait-signal')
    return decoded


def check_deadline(deadline):
    require(deadline is None or (type(deadline) in (int, float) and math.isfinite(deadline)),
            'anchor-deadline')
    require(deadline is None or time.monotonic() < deadline, 'anchor-operation-deadline')


def serialized(method):
    @wraps(method)
    def guarded(self, *args, **kwargs):
        with self._lock:
            if self._busy:
                return self.receipt(reason='anchor-operation-busy', signalAuthority=False,
                                    freshIdentityVerified=False)
            self._busy = True
            try:
                return method(self, *args, **kwargs)
            finally:
                self._busy = False
    return guarded


@dataclass(frozen=True)
class Binding:
    pid: int
    ppid: int
    uid: int
    ruid: int
    pgid: int
    birth: tuple


@dataclass(frozen=True)
class Observation:
    binding: Binding
    state: str
    raw_status: int | None


class GuardedPopen(subprocess.Popen):
    """Actual Popen; standard constructor recovery remains allowed until arm().

    No fake poll result is provided. Armed public/private reaping methods raise.
    An unresolved finalizer warns without touching the OS, never adds to _active.
    Strong registry retains unresolved handles up to MAX_ANCHORS; exhaustion stops
    new work. This explicit quarantine is a limitation, not successful cleanup.
    """
    _armed = False
    _warned = False
    _constructor_started = False

    def arm(self):
        self._armed = True

    def _execute_child(self, *args, **kwargs):
        try:
            return super()._execute_child(*args, **kwargs)
        finally:
            # Explicit exec-error wait belongs to the standard constructor. Once
            # that body returns or is interrupted, no opportunistic reap is allowed.
            if (self._constructor_started or integer(getattr(self, 'pid', None), 1) or
                    getattr(self, '_child_created', False)):
                self.arm()

    def _guard(self):
        if self._armed:
            METRICS['forbiddenPopenCalls'] += 1
            raise AnchorError('anchor-hidden-reap-forbidden')

    def poll(self):
        self._guard()
        return super().poll()

    def wait(self, *args, **kwargs):
        self._guard()
        return super().wait(*args, **kwargs)

    def _wait(self, *args, **kwargs):
        self._guard()
        return super()._wait(*args, **kwargs)

    def _internal_poll(self, *args, **kwargs):
        self._guard()
        return super()._internal_poll(*args, **kwargs)

    def _try_wait(self, *args, **kwargs):
        self._guard()
        return super()._try_wait(*args, **kwargs)

    def _communicate(self, *args, **kwargs):
        self._guard()
        return super()._communicate(*args, **kwargs)

    def send_signal(self, *args, **kwargs):
        self._guard()
        return super().send_signal(*args, **kwargs)

    def communicate(self, *args, **kwargs):
        self._guard()
        return super().communicate(*args, **kwargs)

    def __enter__(self):
        self._guard()
        return super().__enter__()

    def __exit__(self, *args):
        self._guard()
        return super().__exit__(*args)

    def warn_unresolved(self):
        if self._armed and not valid_exit_code(getattr(self, 'returncode', None)) and not self._warned:
            self._warned = True
            METRICS['unresolvedFinalizerWarnings'] += 1
            warnings.warn('anchor-unresolved-quarantine', ResourceWarning, stacklevel=2)

    def __del__(self):
        if (self._armed or self._constructor_started or integer(getattr(self, 'pid', None), 1) or
                getattr(self, '_child_created', False)):
            self._armed = True
            self.warn_unresolved()
        else:
            # __init__/_execute_child failure recovery belongs to standard Popen.
            super().__del__()


class DarwinBSD:
    """Existing BSDInfo + reader.query injection; query itself fixes flavor3 arg1.

    Reader is instantiated outside this primitive. No CDLL/ABI bootstrap fallback,
    API17 authority or task/RSS query is added. Linux tests use an actual BSDInfo
    ctypes type with a fixed query fake, not a claim about Darwin runtime behavior.
    """
    def __init__(self, reader, bsd_type, parent, uid, ruid):
        self.reader, self.bsd_type = reader, bsd_type
        self.parent, self.uid, self.ruid = parent, uid, ruid

    def observe(self, pid, pgid, expected=None, *, deadline=None):
        import ctypes
        require(integer(pid, 1) and type(pgid) is int and pgid == pid,
                'anchor-observation-binding')
        check_deadline(deadline)
        value = self.bsd_type()
        METRICS['bsdQueries'] += 1
        size, error = self.reader.query(pid, 3, value)
        require(type(size) is int and size == ctypes.sizeof(value) and
                type(error) is int and error == 0, 'anchor-bsd-unavailable')
        require(value.pid == pid and value.ppid == self.parent and
                value.uid == self.uid and value.ruid == self.ruid and
                value.pgid == pgid and value.start_sec > 0 and
                0 <= value.start_usec < 1000000 and value.status in (1, 2, 3, 4, 5),
                'anchor-bsd-binding')
        binding = Binding(pid, value.ppid, value.uid, value.ruid, pgid,
                          (value.start_sec, value.start_usec))
        require(expected is None or binding == expected, 'anchor-generation-changed')
        raw = (int(value.xstatus) & 0xffff) if value.status == 5 else None
        if raw is not None:
            decode_status(raw, frozenset(range(1,32)))
        check_deadline(deadline)
        return Observation(binding, 'zombie' if value.status == 5 else 'live', raw)


def _read_small(path):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC)
    try:
        value = os.read(fd, 8193)
        require(len(value) <= 8192, 'anchor-proc-size')
        return value.decode('ascii')
    finally:
        os.close(fd)


def _linux_stat(pid):
    METRICS['procStatReads'] += 1
    text = _read_small(f'/proc/{pid}/stat')
    head, rest = text.rsplit(')', 1)
    require(head.split(' ', 1)[0] == str(pid), 'anchor-proc-binding')
    fields = rest.split()
    require(len(fields) >= 20 and fields[0] in ('R', 'S', 'D', 'T', 't', 'Z', 'X', 'I'),
            'anchor-proc-schema')
    return fields[0], int(fields[1]), int(fields[2]), int(fields[19])


class LinuxWNOWAIT:
    """Real Linux child observation, not a substitute for required Mac proof."""
    def __init__(self):
        self.parent, self.uid, self.ruid = os.getpid(), os.geteuid(), os.getuid()

    def observe(self, pid, pgid, expected=None, *, deadline=None):
        # Only the precise None->Z race gets a second WNOWAIT query. No general
        # fallback, sleep, new clock origin or retry of another identity is used.
        race_binding = expected
        for attempt in range(2):
            check_deadline(deadline)
            METRICS['waitidWNOWAIT'] += 1
            observed = os.waitid(os.P_PID, pid, os.WEXITED | os.WNOWAIT | os.WNOHANG)
            first = _linux_stat(pid)
            METRICS['procStatusReads'] += 1
            rows = _read_small(f'/proc/{pid}/status').splitlines()
            uid_rows = [row.split()[1:] for row in rows if row.startswith('Uid:')]
            require(len(uid_rows) == 1 and len(uid_rows[0]) == 4, 'anchor-proc-schema')
            real, effective = map(int, uid_rows[0][:2])
            second = _linux_stat(pid)
            require(first[1:] == second[1:] and second[1] == self.parent and
                    second[2] == pgid and second[3] > 0 and
                    real == self.ruid and effective == self.uid, 'anchor-proc-binding')
            binding = Binding(pid, second[1], effective, real, pgid, (second[3],))
            require(race_binding is None or race_binding == binding, 'anchor-generation-changed')
            check_deadline(deadline)
            if observed is None and second[0] == 'Z':
                race_binding = binding
                if attempt == 0:
                    METRICS['terminalRaceRetries'] += 1
                    continue
                raise AnchorError('anchor-terminal-race')
            if observed is None:
                return Observation(binding, 'live', None)
            break
        require(observed.si_pid == pid and observed.si_uid == self.ruid,
                'anchor-waitid-binding')
        require(second[0] == 'Z', 'anchor-terminal-race')
        code, status = observed.si_code, observed.si_status
        require(type(status) is int, 'anchor-wait-status')
        if code == os.CLD_EXITED:
            require(0 <= status <= 255, 'anchor-wait-status')
            raw = status << 8
        else:
            require(code in (os.CLD_KILLED, os.CLD_DUMPED) and 0 < status < 127,
                    'anchor-wait-status')
            raw = status | (128 if code == os.CLD_DUMPED else 0)
        decode_status(raw)
        return Observation(binding, 'zombie', raw)


class LinuxProcfsCensus:
    """Bounded, non-atomic Linux snapshot for isolated tests only.

    Scans numeric proc entries, retaining every observed group zombie. Disappeared
    entries can be skipped; membership can move concurrently, so this cannot prove
    session/descendant closure or rule out same-SID leave/rejoin. No signals here.
    """
    def __call__(self, pgid, *, deadline=None):
        check_deadline(deadline)
        METRICS['rawCensusCalls'] += 1
        pids = []
        with os.scandir('/proc') as entries:
            for count, entry in enumerate(entries, 1):
                check_deadline(deadline)
                require(count <= MAX_ROSTER, 'anchor-census-limit')
                if not entry.name.isdecimal():
                    continue
                pid = int(entry.name)
                try:
                    row = _linux_stat(pid)
                except FileNotFoundError:
                    continue
                if row[2] == pgid:
                    pids.append(pid)
        return {'status': 'complete', 'group': pgid, 'pids': sorted(pids),
                'source': 'linux-procfs-test', 'bindingVerified': True}


def raw_roster(value, pgid, bootstrap_stage=None):
    require(type(value) is dict and set(value) ==
            {'status', 'group', 'pids', 'source', 'bindingVerified'}, 'anchor-census-schema')
    require(type(value['status']) is str and value['status'] == 'complete' and type(value['group']) is int and
            value['group'] == pgid and value['bindingVerified'] is True and
            type(value['source']) is str,
            'anchor-census-unavailable')
    source = value['source']
    require((sys.platform == 'darwin' and source == 'darwin-header-bound') or
            (sys.platform == 'darwin' and source == 'darwin-bootstrap-ps' and
             bootstrap_stage in ('census-abi-compile', 'census-abi')) or
            (sys.platform == 'linux' and source in ('linux-procfs-test', 'linux-ps-test')),
            'anchor-census-source')
    pids = value['pids']
    require(type(pids) is list and len(pids) <= MAX_ROSTER and
            all(integer(pid, 1) for pid in pids) and len(set(pids)) == len(pids),
            'anchor-census-pids')
    return frozenset(pids)


class PrimaryAnchor:
    def __init__(self, process, backend, census):
        self._process, self._backend, self._census = process, backend, census
        self._lock = threading.RLock()
        self._pid = getattr(process, 'pid', None) if process is not None else None
        self._binding = None
        self._state = 'held'
        self._actual_reaped = None
        self._exit_code = None
        self._reason = None
        self._resources = {'cpuSeconds': None, 'rssMiB': None}
        self._calls = Counter()
        self._busy = False
        self._last_observed_exit = None
        self._bootstrap_stage = None
        self._last_error = {'errorType': None, 'errno': None}

    @property
    def pid(self):
        return self._pid

    @property
    def process(self):
        return self._process

    @property
    def actual_returncode(self):
        value = getattr(self._process, 'returncode', None) if self._process is not None else None
        return value if valid_exit_code(value) else None

    @property
    def observed_exit_code(self):
        return self._last_observed_exit

    @property
    def held(self):
        return self._state == 'held'

    @property
    def lifecycle_state(self):
        return self._state

    @classmethod
    def spawn(cls, argv, *, cwd=None, env=None, stdin=None, stdout=None, stderr=None,
              backend=None, census=None, bootstrap_stage=None):
        require(type(argv) is list and bool(argv) and
                all(type(item) is str and '\0' not in item for item in argv), 'anchor-argv')
        require(bootstrap_stage is None or bootstrap_stage in
                ('census-abi-compile', 'census-abi'), 'anchor-bootstrap-stage')
        sole_reaper_check()
        if backend is None:
            if sys.platform == 'darwin':
                from darwin_activity import BSDInfo, probe
                backend = DarwinBSD(probe(), BSDInfo, os.getpid(), os.geteuid(), os.getuid())
            else:
                require(sys.platform == 'linux', 'anchor-platform')
                backend = LinuxWNOWAIT()
        # Allocate lifecycle state before creation, so constructor errors own no child.
        anchor = cls(None, backend, census)
        anchor._bootstrap_stage = bootstrap_stage
        with _registry_lock:
            require(len(retained_anchors()) < MAX_ANCHORS, 'anchor-quarantine-limit')
            # Constructor failure retains normal Popen internal recovery semantics.
            # Allocate before __init__, retaining the actual instance even when
            # initialization is interrupted after fork. No synthetic Popen facade.
            process = GuardedPopen.__new__(GuardedPopen)
            anchor._process, anchor._pid = process, getattr(process, 'pid', None)
            opaque_key = id(process)
            try:
                # Strong ownership precedes constructor entry. A fork-return /
                # STORE_ATTR interruption can leave no Python-visible PID.
                _opaque_registry[opaque_key] = anchor
                process._constructor_started = True
                process.__init__(argv, cwd=cwd, env=env, stdin=stdin, stdout=stdout,
                                 stderr=stderr, start_new_session=True)
                process.arm()
                require(integer(getattr(process, 'pid', None), 1), 'anchor-constructor-failed')
                anchor._pid = process.pid
                _registry[process.pid] = anchor
                _opaque_registry.pop(opaque_key, None)
            except BaseException as error:
                pid = getattr(process, 'pid', None)
                # Never infer no child from _child_created=False or a missing PID.
                # Keep pending ownership during error processing as well.
                process._constructor_started = True
                process.arm()
                anchor._state = 'lost'
                anchor._set_error(error)
                anchor._reason = 'anchor-constructor-interrupted' if isinstance(
                    error, (KeyboardInterrupt, SystemExit)) else 'anchor-constructor-failed'
                if integer(pid, 1):
                    anchor._pid = pid
                    if anchor.actual_returncode is None:
                        _registry[pid] = anchor
                    else:
                        # Standard explicit exec-error wait already completed.
                        anchor._actual_reaped = True
                    _opaque_registry.pop(opaque_key, None)
                else:
                    anchor._pid = None
                    _opaque_registry[opaque_key] = anchor
                if isinstance(error, (KeyboardInterrupt, SystemExit)):
                    raise AnchorInterrupted('anchor-constructor-interrupted', anchor) from None
                raise AnchorSpawnError('anchor-constructor-failed', anchor) from None
        # Post-creation ordinary capture errors return the retained handle.
        anchor.observe()
        return anchor

    def _revoke(self, reason, actual_reaped=None):
        self._state, self._reason = 'lost', reason
        self._actual_reaped = actual_reaped

    def _close_input(self):
        if self._process.stdin is not None and not self._process.stdin.closed:
            self._process.stdin.close()

    def close_input(self):
        with self._lock:
            self._close_input()

    def _observation(self, deadline=None):
        sole_reaper_check()
        require(self._state == 'held', 'anchor-authority-revoked')
        require(self._process.returncode is None, 'anchor-popen-status-mutated')
        self._calls['observe'] += 1
        check_deadline(deadline)
        args = {} if deadline is None else {'deadline': deadline}
        observed = self._backend.observe(self._pid, self._pid, self._binding, **args)
        require(type(observed) is Observation and type(observed.binding) is Binding and
                all(type(item) is int for item in (observed.binding.pid,observed.binding.ppid,
                    observed.binding.uid,observed.binding.ruid,observed.binding.pgid)) and
                observed.binding.pid == self._pid and observed.binding.pgid == self._pid and
                observed.binding.ppid == os.getpid() and
                observed.binding.uid == os.geteuid() and observed.binding.ruid == os.getuid(),
                'anchor-observation-binding')
        require(type(observed.binding.birth) is tuple and len(observed.binding.birth) in (1,2) and
                all(type(item) is int for item in observed.binding.birth) and
                observed.binding.birth[0] > 0 and
                (len(observed.binding.birth)==1 or 0<=observed.binding.birth[1]<1000000),
                'anchor-observation-binding')
        require(observed.state in ('live', 'zombie') and
                ((observed.state == 'live' and observed.raw_status is None) or
                 (observed.state == 'zombie' and type(observed.raw_status) is int)),
                'anchor-observation-schema')
        if self._binding is not None:
            require(self._binding == observed.binding, 'anchor-generation-changed')
        self._binding = observed.binding
        if observed.state == 'zombie':
            self._last_observed_exit = decode_status(observed.raw_status)
        else:
            self._last_observed_exit = None
        self._clear_error()
        return observed

    def _clear_error(self):
        self._last_error = {'errorType': None, 'errno': None}

    def _set_error(self, error):
        kind = ('os-error' if isinstance(error, OSError) else
                'anchor-error' if isinstance(error, AnchorError) else
                'interrupted' if isinstance(error, (KeyboardInterrupt, SystemExit)) else 'exception')
        value = getattr(error, 'errno', None)
        self._last_error = {'errorType': kind, 'errno': value if integer(value, 0) else None}

    def _error(self, error):
        self._set_error(error)
        self._last_observed_exit = None
        if isinstance(error, ChildProcessError) or (
                isinstance(error, OSError) and error.errno == errno.ECHILD):
            self._revoke('anchor-echild')
            return 'anchor-echild'
        if isinstance(error, AnchorError):
            code = str(error)
            if code not in FIXED_CODES:
                return 'anchor-observation-unavailable'
            if code in ('anchor-generation-changed', 'anchor-observation-binding',
                        'anchor-popen-status-mutated','anchor-bsd-binding','anchor-proc-binding',
                        'anchor-sigchld-not-default'):
                self._revoke(code)
            return code
        return 'anchor-observation-unavailable'

    @serialized
    def observe(self, *, deadline=None):
        with self._lock:
            if self._state != 'held':
                return self.receipt()
            try:
                observation = self._observation(deadline)
                return self.receipt(status=observation.state, observedState=observation.state, reason=None,
                                    freshIdentityVerified=True, signalAuthority=True,
                                    observedExitCode=(decode_status(observation.raw_status)
                                                      if observation.state == 'zombie' else None))
            except Exception as error:
                return self.receipt(status='unavailable', reason=self._error(error), freshIdentityVerified=False,
                                    signalAuthority=False)

    def receipt(self, **extra):
        return {'status': self._state if self._state in ('reaped','lost','recovered') else 'unavailable',
                'state': self._state, 'reason': self._reason,
                'actualReaped': self._actual_reaped, 'exitCode': self._exit_code,
                'actualReturncode': self.actual_returncode, 'observedExitCode': self._last_observed_exit,
                'pidKnown': integer(self._pid, 1),
                'childExistence': 'observed' if integer(self._pid, 1) else 'unconfirmed',
                'retainedActualPopen': True,
                'anchorReservation': ('conditional-on-sole-reaper' if self._state == 'held'
                                      else 'revoked'),
                'signalAuthority': None if self._state == 'held' else False,
                'freshIdentityVerified': None,
                **self._last_error,
                'resources': dict(self._resources), 'scope': 'active-process-group-epoch',
                'sessionOrDescendantClosure': 'unconfirmed',
                'groupGone': None, 'cleanupStatus': 'unconfirmed', **extra}

    @serialized
    def signal_group(self, sig, *, deadline=None):
        with self._lock:
            if self._state != 'held':
                return self.receipt(reason='anchor-authority-revoked')
            try:
                require(type(sig) is int and sig in
                        (int(signal.SIGINT), int(signal.SIGTERM), int(signal.SIGKILL)), 'anchor-signal')
                self._observation(deadline)
                check_deadline(deadline)
                sole_reaper_check()
                require(self._state == 'held' and self._process.returncode is None,
                        'anchor-popen-status-mutated')
                self._calls['killpg'] += 1
                METRICS['killpg'] += 1
                os.killpg(self._pid, sig)
                self._clear_error()
                return self.receipt(reason='anchor-signal-sent', freshIdentityVerified=True,
                                    signalAuthority=True)
            except Exception as error:
                return self.receipt(reason=self._error(error), freshIdentityVerified=False,
                                    signalAuthority=False)

    @serialized
    def try_reap(self, census=None, *, deadline=None):
        with self._lock:
            if self._state != 'held':
                return self.receipt()
            try:
                first = self._observation(deadline)
                if first.state != 'zombie':
                    return self.receipt(reason='anchor-not-terminal')
                self._calls['census'] += 1
                callback = self._census if census is None else census
                require(callable(callback), 'anchor-census-unavailable')
                members = raw_roster(callback(self._pid, deadline=deadline), self._pid,
                                     self._bootstrap_stage)
                check_deadline(deadline)
                require(self._pid in members, 'anchor-missing-from-roster')
                if members != {self._pid}:
                    return self.receipt(reason='anchor-nonleaders-remain')
                last = self._observation(deadline)
                require(last.state == 'zombie' and last.binding == first.binding and
                        last.raw_status == first.raw_status, 'anchor-terminal-mismatch')
                require(self._state == 'held' and self._process.returncode is None,
                        'anchor-popen-status-mutated')
                # Irreversible authority revocation precedes the first reap syscall.
                self._state = 'reaping'
                for attempt in range(2):
                    check_deadline(deadline)
                    sole_reaper_check()
                    self._calls['waitpid'] += 1
                    METRICS['exactWaitpid'] += 1
                    try:
                        waited, raw = os.waitpid(self._pid, os.WNOHANG)
                        break
                    except InterruptedError:
                        if attempt:
                            raise AnchorError('anchor-final-wait-interrupted')
                if type(waited) is not int:
                    self._revoke('anchor-wait-pid-mismatch')
                    return self.receipt()
                if waited == 0:
                    self._revoke('anchor-wait-not-ready')
                    return self.receipt()
                if type(waited) is not int or waited != self._pid:
                    self._revoke('anchor-wait-pid-mismatch')
                    return self.receipt()
                # Positive exact actual wait is irreversible, even if later checks fail.
                self._actual_reaped = True
                actual = decode_status(raw)
                self._process.returncode = actual
                if raw != last.raw_status:
                    self._revoke('anchor-wait-status-mismatch', actual_reaped=True)
                    return self.receipt()
                self._state, self._exit_code, self._reason = 'reaped', actual, None
                self._clear_error()
                with _registry_lock:
                    _registry.pop(self._pid, None)
                return self.receipt()
            except Exception as error:
                # A positive wait followed by malformed status still revokes signal authority.
                if self._actual_reaped is True:
                    self._revoke('anchor-wait-status', actual_reaped=True)
                elif self._state == 'reaping':
                    self._revoke('anchor-final-wait-unavailable')
                code = self._error(error)
                if self._state == 'lost' and code == 'anchor-observation-unavailable':
                    code = self._reason
                return self.receipt(reason=code, signalAuthority=False)

    def post_reap_absence(self, census=None, *, deadline=None):
        """Read only; never restores authority and cannot prove no escaped children."""
        with self._lock:
            require(self._state != 'held', 'anchor-still-held')
            try:
                require(integer(self._pid, 1), 'anchor-recovery-unbound')
                check_deadline(deadline)
                callback = self._census if census is None else census
                require(callable(callback), 'anchor-census-unavailable')
                members = raw_roster(callback(self._pid, deadline=deadline), self._pid,
                                     self._bootstrap_stage)
                check_deadline(deadline)
                self._clear_error()
                return {'status': 'observed', 'groupEmpty': not members,
                        'readOnly': True, 'signalAuthority': False, **self._last_error}
            except Exception as error:
                self._set_error(error)
                return {'status': 'unconfirmed', 'groupEmpty': None,
                        'readOnly': True, 'signalAuthority': False, **self._last_error}

    @serialized
    def recover_exact_child(self, *, deadline=None):
        """Nonblocking, no-signal recovery; never supplies a group-cleanup proof.

        No numeric wait after ECHILD or replacement evidence, or without an original
        birth. A still-bound child may be exactly waited after authority was revoked.
        """
        with self._lock:
            if self._actual_reaped is True or self._state in ('reaped', 'recovered'):
                return self.receipt()
            if self._reason in ('anchor-echild', 'anchor-generation-changed',
                                'anchor-bsd-binding', 'anchor-proc-binding',
                                'anchor-observation-binding'):
                return self.receipt(signalAuthority=False)
            if self._binding is None:
                self._revoke('anchor-recovery-unbound')
                return self.receipt(signalAuthority=False)
            self._state = 'recovering'
            try:
                check_deadline(deadline)
                sole_reaper_check()
                args = {} if deadline is None else {'deadline': deadline}
                observed = self._backend.observe(self._pid, self._pid, self._binding, **args)
                require(type(observed) is Observation and observed.binding == self._binding,
                        'anchor-generation-changed')
                require(observed.state == 'zombie', 'anchor-recovery-not-terminal')
                expected_raw = observed.raw_status
                decode_status(expected_raw)
                check_deadline(deadline)
                sole_reaper_check()
                self._calls['recoveryWaitpid'] += 1
                METRICS['recoveryWaitpid'] += 1
                waited, raw = os.waitpid(self._pid, os.WNOHANG)
                require(type(waited) is int and waited == self._pid,
                        'anchor-recovery-not-ready' if waited == 0 else 'anchor-wait-pid-mismatch')
                self._actual_reaped = True
                actual = decode_status(raw)
                self._process.returncode = actual
                require(raw == expected_raw, 'anchor-wait-status-mismatch')
                self._state, self._exit_code = 'recovered', actual
                self._reason = 'anchor-recovered-without-group-proof'
                self._clear_error()
                with _registry_lock:
                    _registry.pop(self._pid, None)
                return self.receipt(signalAuthority=False)
            except Exception as error:
                self._revoke('anchor-recovery-unavailable', actual_reaped=self._actual_reaped)
                code = self._error(error)
                if code == 'anchor-observation-unavailable':
                    code = self._reason
                return self.receipt(reason=code, signalAuthority=False)


def quarantine_receipt():
    with _registry_lock:
        anchors = retained_anchors()
        return {'capacity': MAX_ANCHORS, 'retained': len(anchors),
                'opaque': len(_opaque_registry),
                'lost': sum(anchor._state == 'lost' for anchor in anchors),
                'status': 'unconfirmed' if _registry or _opaque_registry else 'empty'}

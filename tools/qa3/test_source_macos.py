# SPDX-License-Identifier: MPL-2.0

"""macOS session port of the QA3 owned-process supervisor contract.

The Linux supervisor (\`\`test_source.py\`\`) and the Windows Job Object port
(\`\`test_source_windows.py\`\`) promise three things: collect every descendant,
never stop unrelated processes, and treat incomplete cleanup as a fail
verdict. These tests pin the same three promises for the macOS session
supervisor, plus the lessons of the native PR #3139 run: the process listing
is uid-scoped (\`\`proc_listpids PROC_UID_ONLY\`\`) and keeps protected pids as
explicit unobservable entries instead of failing the whole scan, and an
un-reaped zombie leader may lose its BSD description although the held Popen
still reserves the kernel PID. The contract classes run everywhere against
an in-memory process API, so the decision logic is covered on any host;
\`\`MacosSessionTests\`\` runs the real libproc path on macOS.
"""

import ctypes
import errno
import json
import os
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

import source

MACOS = sys.platform == "darwin"
# The fake processes belong to this uid; the supervisor under test is told so.
UID = 501
BORN = (2_000_000_000, 0)


class _FakeProcess:
    """Popen parity: the exit status is cached only when this object reaps.

    \`\`api.exit_codes\`\` records the kernel status, but \`\`returncode\`\` stays
    \`\`None\`\` until \`\`poll\`\`/\`\`wait\`\` runs, exactly like an un-reaped Popen
    whose kernel PID reservation is still held.
    """

    def __init__(self, api, pid):
        self._api = api
        self.pid = pid
        self.signals = []
        self._reaped = False

    def poll(self):
        # The only reap point: a real Popen caches the status here and the
        # kernel releases the PID reservation.
        self._reaped = True
        code = self.returncode
        entry = self._api.procs.get(self.pid)
        if code is not None and entry is not None and entry[3] == BORN:
            self._api.procs.pop(self.pid)
        return code

    def wait(self, timeout=None):
        return self.poll()

    @property
    def returncode(self):
        if not self._reaped:
            return None
        return self._api.exit_codes.get(self.pid)

    def send_signal(self, sig):
        self.signals.append(sig)


class _FakeMacosApi:
    """In-memory stand-in for the libproc/getsid primitives.

    \`\`procs\`\` maps pid to \`\`(ppid, uid, session, identity)\`\`. \`\`spawn_pid\`\` is
    the command the supervisor starts; it leads its own session and the
    spawn-time identity read still sees it alive, like a just-executed child.
    Once the supervisor starts scanning, an exit-coded leader models the
    command that finished before cleanup: natively its un-reaped zombie loses
    its BSD description (\`\`proc_pidinfo\`\` answers ESRCH) although the held
    Popen still reserves the PID, so it stays out of \`\`process_table\`\` and
    \`\`identity\`\` until poll reaps it.

    Denial fixtures: \`\`denied\`\` pids are BSD-undescribable in the scan and
    are reported as fresh \`\`(pid, errno.EPERM)\`\` unobservable entries per
    scan; \`\`rebound\`\` maps a denied pid to the birth a direct inspection
    reads after the scan (permission state may change between the two);
    \`\`ghost\`\` pids are denied for direct inspection yet absent from every
    listing; \`\`session_denied\`\` pids also deny \`\`getsid\`\`.
    """

    def __init__(self, procs=None, spawn_pid=900000, now=BORN, denied=(),
                 session_denied=(), ghost=()):
        self.procs = dict(procs or {})
        self.exit_codes = {}
        self.spawn_pid = spawn_pid
        self.clock = now
        self.signalled = []
        self.checked = False
        self.denied = set(denied)
        self.session_denied = set(session_denied)
        self.ghost = set(ghost)
        self.rebound = {}
        self.unobservable = []
        self.leader_exited = False
        self.scanned = False

    def check(self):
        self.checked = True

    def now(self):
        return self.clock

    def spawn(self, argv):
        self.procs[self.spawn_pid] = (os.getpid(), UID, self.spawn_pid, BORN)
        process = _FakeProcess(self, self.spawn_pid)
        self.exit_codes[self.spawn_pid] = 0
        # A real child remains an identity-bearing zombie until Popen.poll
        # reaps it; natively its BSD description may vanish meanwhile.
        return process

    def _leader_zombie(self, pid):
        # Observed natively: an un-reaped Popen zombie may answer
        # proc_pidinfo with ESRCH although the PID is still reserved.
        entry = self.procs.get(pid)
        return (
            entry is not None
            and pid == self.spawn_pid
            and self.leader_exited
            and entry[3] == BORN
        )

    def _deny(self, pid):
        if pid in self.ghost or (
            pid in self.denied and pid not in self.rebound
        ):
            raise PermissionError(errno.EPERM, "fixture denies the inspection")

    def process_table(self):
        self.scanned = True
        # Fixture fiction: the spawned command is instantaneous, so once the
        # supervisor starts scanning, an exit-coded leader is an un-reaped
        # zombie whose BSD description the kernel has already dropped.
        if self.spawn_pid in self.exit_codes:
            self.leader_exited = True
        # Fresh per scan, like the real API: never appended to, never stale.
        self.unobservable = [(pid, errno.EPERM) for pid in sorted(self.denied)]
        table = {}
        for pid, (ppid, uid, _session, identity) in self.procs.items():
            if self._leader_zombie(pid) or pid in self.denied:
                continue
            table[pid] = (ppid, uid, 0, identity)
        return table

    def identity(self, pid):
        self._deny(pid)
        if pid in self.rebound:
            return self.rebound[pid]
        entry = self.procs.get(pid)
        if entry is None or self._leader_zombie(pid):
            return None
        return entry[3]

    def session(self, pid):
        if pid in self.session_denied:
            raise PermissionError(errno.EPERM, "fixture denies the session")
        entry = self.procs.get(pid)
        return None if entry is None else entry[2]

    def is_alive(self, pid, identity=None):
        self._deny(pid)
        entry = self.procs.get(pid)
        if entry is None or self._leader_zombie(pid):
            return False
        return identity is None or entry[3] == identity

    def terminate(self, pid, identity, sig):
        self._deny(pid)
        entry = self.procs.get(pid)
        if entry is None or self._leader_zombie(pid) or entry[3] != identity:
            return
        self.signalled.append((pid, sig))
        del self.procs[pid]


class _UndescribedSpawnApi(_FakeMacosApi):
    """The spawn-time identity read fails, as in a zombie race at exec."""

    def identity(self, pid):
        if pid == self.spawn_pid and not self.scanned:
            return None
        return super().identity(pid)


def _later(seconds=5):
    return (BORN[0] + seconds, 0)



class MacosSupervisorContractTests(unittest.TestCase):
    def _supervisor(self, api):
        supervisor = source.MacosSessionSupervisor(api, cleanup_interval=0.05)
        supervisor.uid = UID
        return supervisor

    def _run(self, api):
        supervisor = self._supervisor(api)
        supervisor.prepare()
        supervisor.spawn([sys.executable, "-c", "pass"])
        return supervisor

    def test_supervisor_kind_selects_the_session_supervisor_on_macos(self):
        self.assertEqual(source.supervisor_kind("posix", "darwin"), "macos")
        self.assertEqual(source.supervisor_kind("posix", "linux"), "linux")
        self.assertEqual(source.supervisor_kind("nt", "darwin"), "windows")
        if MACOS:
            self.assertIsInstance(
                source.owned_supervisor(), source.MacosSessionSupervisor
            )
        else:
            with self.assertRaises(OSError):
                source.MacosProcessApi()

    def test_every_descendant_is_collected_and_unrelated_processes_survive(self):
        api = _FakeMacosApi(
            procs={
                300: (200, UID, 200, _later()),
                301: (300, UID, 200, _later()),
                400: (1, UID, 400, _later()),
            },
            spawn_pid=200,
        )
        supervisor = self._run(api)
        complete, errors = supervisor.cleanup()
        self.assertTrue(api.checked)
        self.assertTrue(complete, errors)
        self.assertEqual(errors, [])
        self.assertEqual(
            sorted(pid for pid, _sig in api.signalled), [300, 301]
        )
        self.assertIn(400, api.procs, "an unrelated process was stopped")

    def test_an_orphan_reparented_to_launchd_is_collected_by_its_session(self):
        api = _FakeMacosApi(
            procs={301: (1, UID, 200, _later())},
            spawn_pid=200,
        )
        supervisor = self._run(api)
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual([pid for pid, _sig in api.signalled], [301])

    def test_a_descendant_seen_before_it_left_the_session_stays_owned(self):
        api = _FakeMacosApi(
            procs={301: (200, UID, 200, _later())},
            spawn_pid=200,
        )
        supervisor = self._supervisor(api)
        supervisor.prepare()
        supervisor.spawn([sys.executable, "-c", "pass"])
        # The command is still running while the supervisor tracks it.
        api.procs[200] = (os.getpid(), UID, 200, BORN)
        supervisor.owned[200] = BORN
        supervisor.track()
        # Then the descendant calls setsid and its parent exits.
        api.procs[301] = (1, UID, 301, _later())
        del api.procs[200]
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual([pid for pid, _sig in api.signalled], [301])

    def test_session_members_of_another_user_or_born_earlier_are_not_owned(self):
        api = _FakeMacosApi(
            procs={
                301: (1, UID + 1, 200, _later()),
                302: (1, UID, 200, (BORN[0] - 60, 0)),
            },
            spawn_pid=200,
        )
        supervisor = self._run(api)
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual(api.signalled, [])
        self.assertIn(301, api.procs)
        self.assertIn(302, api.procs)

    def test_a_reused_process_identifier_is_not_signalled(self):
        api = _FakeMacosApi(
            procs={301: (200, UID, 200, _later())},
            spawn_pid=200,
        )
        supervisor = self._run(api)
        supervisor.track()
        # pid 301 exits and is reused by an unrelated process.
        api.procs[301] = (1, UID, 301, _later(9))
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual(api.signalled, [])
        self.assertIn(301, api.procs)

    def test_a_child_of_a_reused_parent_identifier_is_not_owned(self):
        api = _FakeMacosApi(procs={}, spawn_pid=200)
        supervisor = self._run(api)
        supervisor.owned[300] = _later()
        # pid 300 now names an unrelated process with a child of its own.
        api.procs[300] = (1, UID, 300, _later(9))
        api.procs[301] = (300, UID, 300, _later(10))
        supervisor.track()
        self.assertNotIn(301, supervisor.owned)

    def test_an_owned_descendant_that_survives_cleanup_fails_the_verdict(self):
        class SurvivingApi(_FakeMacosApi):
            def terminate(self, pid, identity, sig):
                if pid == 301:
                    raise OSError("fixture keeps an owned descendant alive")
                super().terminate(pid, identity, sig)

        api = SurvivingApi(
            procs={301: (200, UID, 200, _later())},
            spawn_pid=200,
        )
        supervisor = self._supervisor(api)
        # Nested, not parenthesised: macOS ships Python 3.9.
        with patch.object(source, "owned_supervisor", return_value=supervisor):
            with patch.object(source, "_install_stop_handlers", lambda stop: None):
                result = source.run_owned_supervisor(
                    5.0, [sys.executable, "-c", "pass"]
                )
        self.assertEqual(result["exitCode"], 0)
        self.assertFalse(result["ownedCleanupComplete"])
        self.assertIn(
            "fixture keeps an owned descendant alive",
            result["cleanupErrors"][0],
        )
        self.assertEqual(result["verdict"], "FAIL")
        self.assertIsNotNone(source.supervisor_failure(result))
        self.assertIn(301, api.procs, "the surviving descendant was not verified")

    def test_an_unprepared_supervisor_cannot_report_a_clean_result(self):
        api = _FakeMacosApi(spawn_pid=200)
        supervisor = self._supervisor(api)
        supervisor.process = _FakeProcess(api, 200)
        complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(errors)

    def test_an_empty_session_is_not_matched_once_its_leader_is_reaped(self):
        api = _FakeMacosApi(procs={}, spawn_pid=200)
        supervisor = self._run(api)
        supervisor.track()
        # Explicit reap: while the Popen is un-reaped its reservation holds
        # the session id, so the empty scan must not drop it yet.
        self.assertEqual(supervisor.process.poll(), 0)
        supervisor.track()
        self.assertIsNone(supervisor.session)
        # pid 200 is reused by a new session leader of the same user.
        api.procs[200] = (1, UID, 200, _later(9))
        api.procs[201] = (200, UID, 200, _later(10))
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual(api.signalled, [])
        self.assertIn(201, api.procs)

    def test_a_live_member_of_another_user_keeps_the_session_open(self):
        api = _FakeMacosApi(
            procs={301: (1, UID + 1, 200, _later())},
            spawn_pid=200,
        )
        supervisor = self._run(api)
        supervisor.track()
        self.assertEqual(supervisor.session, 200)
        self.assertNotIn(301, supervisor.owned)
        # It forks a child of this user, which is owned.
        api.procs[302] = (301, UID, 200, _later(2))
        supervisor.track()
        self.assertIn(302, supervisor.owned)

    def test_the_leaders_start_time_bounds_session_members(self):
        class LiveLeaderApi(_FakeMacosApi):
            def spawn(self, argv):
                self.procs[self.spawn_pid] = (os.getpid(), UID, 200, _later(3))
                return _FakeProcess(self, self.spawn_pid)

        # The wall clock is far behind the kernel start times.
        api = LiveLeaderApi(procs={}, spawn_pid=200, now=(1, 0))
        supervisor = self._run(api)
        self.assertEqual(supervisor.born_after, _later(3))
        api.procs[301] = (1, UID, 200, _later(4))
        api.procs[302] = (1, UID, 200, _later(1))
        supervisor.track()
        self.assertIn(301, supervisor.owned)
        self.assertNotIn(302, supervisor.owned)

    def test_a_new_process_on_a_dead_owned_pid_is_judged_afresh(self):
        api = _FakeMacosApi(procs={}, spawn_pid=200)
        supervisor = self._run(api)
        supervisor.owned[301] = _later()
        supervisor.session = 200
        api.procs[301] = (1, UID, 200, _later(9))
        supervisor.track()
        self.assertEqual(supervisor.owned[301], _later(9))

    def test_the_running_command_is_signalled_term_then_kill(self):
        class StubbornProcess(_FakeProcess):
            @property
            def returncode(self):
                return 0 if source._MAC_CLEANUP_SIGNALS[1] in self.signals else None

        class StubbornApi(_FakeMacosApi):
            def spawn(self, argv):
                return StubbornProcess(self, self.spawn_pid)

        api = StubbornApi(spawn_pid=200)
        supervisor = self._run(api)
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual(supervisor.process.signals[0], signal.SIGTERM)
        self.assertIn(source._MAC_CLEANUP_SIGNALS[1], supervisor.process.signals)


    def test_a_held_leader_anchors_the_first_session_scan(self):
        # Native regression: the un-reaped zombie may have no BSD description
        # at all; the held Popen reservation must still anchor the session so
        # the first scan adopts same-uid members born after the spawn.
        api = _FakeMacosApi(procs={301: (1, UID, 200, _later())}, spawn_pid=200)
        supervisor = self._supervisor(api)
        supervisor.prepare()
        supervisor.spawn([sys.executable, "-c", "pass"])
        supervisor.track()
        self.assertIsNone(api.identity(200), "the zombie regained a description")
        self.assertNotIn(200, api.process_table())
        self.assertEqual(set(supervisor.owned), {200, 301})
        self.assertIsNone(
            supervisor.process.returncode, "the leader was reaped"
        )

    def test_an_undescribed_hidden_leader_invents_no_birth(self):
        # The spawn-time read failed and the held zombie stays invisible: no
        # birth is guessed, so no member birth bound exists at all; the held
        # reservation alone anchors the session generation.
        api = _UndescribedSpawnApi(
            procs={301: (1, UID, 200, (BORN[0] - 60, 0))}, spawn_pid=200
        )
        supervisor = self._supervisor(api)
        supervisor.prepare()
        supervisor.spawn([sys.executable, "-c", "pass"])
        self.assertIsNone(supervisor.born_after)
        supervisor.track()
        self.assertIn(301, supervisor.owned)
        self.assertIsNone(supervisor.leader_identity)
        self.assertIsNone(supervisor.born_after)

    def test_a_held_slot_birth_is_recorded_from_the_first_scan(self):
        api = _UndescribedSpawnApi(procs={}, spawn_pid=200)
        supervisor = self._supervisor(api)
        supervisor.prepare()
        supervisor.spawn([sys.executable, "-c", "pass"])
        self.assertIsNone(supervisor.leader_identity)
        self.assertIsNone(supervisor.born_after)
        del api.exit_codes[200]  # the fixture's command is still running
        api.procs[301] = (200, UID, 200, _later())
        supervisor.track()
        # The first scan reads the held slot's real birth; nothing is
        # invented, and that birth becomes the session member bound.
        self.assertEqual(supervisor.leader_identity, BORN)
        self.assertEqual(supervisor.born_after, BORN)
        self.assertIn(301, supervisor.owned)

    def test_a_foreign_birth_in_the_held_leader_slot_fails_closed(self):
        api = _FakeMacosApi(procs={}, spawn_pid=200)
        supervisor = self._supervisor(api)
        supervisor.prepare()
        supervisor.spawn([sys.executable, "-c", "pass"])
        del api.exit_codes[200]  # describe the held slot alive again
        api.procs[301] = (200, UID, 200, _later())
        supervisor.track()
        self.assertEqual(supervisor.leader_identity, BORN)
        # A different birth occupies the slot while the Popen is unreaped:
        # the PID cannot have changed occupant, so this fails closed.
        api.procs[200] = (1, UID, 200, _later(9))
        complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(
            any("identity changed while unreaped" in e for e in errors)
        )
        self.assertIsNone(supervisor.session)
        self.assertFalse(
            any(pid == 200 for pid, _sig in api.signalled),
            "the replacement occupant was signalled",
        )
        self.assertIn(200, api.procs)

    def test_the_fake_process_caches_none_until_poll_reaps(self):
        api = _FakeMacosApi(spawn_pid=200)
        supervisor = self._run(api)
        process = supervisor.process
        # The kernel status exists, but an un-reaped Popen must not expose it.
        self.assertEqual(api.exit_codes[200], 0)
        self.assertIsNone(process.returncode)
        self.assertIn(200, api.procs)  # the table entry is retained
        self.assertEqual(process.poll(), 0)  # poll is the only reap point
        self.assertEqual(process.returncode, 0)  # pinned after the reap
        self.assertEqual(process.poll(), 0)
        self.assertNotIn(200, api.procs)


def _wait_dead(pid, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not source.MacosProcessApi().is_alive(pid):
            return True
        time.sleep(0.05)
    return False


def _orphan_command(marker, leave_session=False):
    """A command whose child outlives it, reparented to launchd.

    With \`\`leave_session\`\` the child first waits to be seen, then calls
    setsid, so only the ancestry rule can still own it.
    """
    child = (
        "import os,pathlib,time\n"
        f"marker = pathlib.Path({str(marker)!r})\n"
        + (
            "time.sleep(0.5)\n"
            "os.setsid()\n"
            if leave_session
            else ""
        )
        + "marker.write_text(str(os.getpid()))\n"
        "time.sleep(30)\n"
    )
    parent = (
        "import pathlib,subprocess,sys,time\n"
        f"marker = pathlib.Path({str(marker)!r})\n"
        # The orphan must not hold the worker's output pipe open, or the test
        # would wait for it to exit on its own.
        f"subprocess.Popen([sys.executable,'-I','-c',{child!r}],"
        "stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,"
        "stderr=subprocess.DEVNULL)\n"
        "deadline = time.monotonic() + 10\n"
        "while not marker.exists() and time.monotonic() < deadline:\n"
        "    time.sleep(0.01)\n"
        "assert marker.exists()\n"
    )
    return [sys.executable, "-I", "-c", parent]


WORKER = """
import json, pathlib, sys
sys.path.insert(0, str(pathlib.Path(sys.argv[2]) / "tools" / "qa3"))
import source
command = json.loads(pathlib.Path(sys.argv[3]).read_text())
source._execute_owned(pathlib.Path(sys.argv[1]), float(sys.argv[4]), command)
"""

WORKER_WITH_CLEANUP_FAULT = """
import json, pathlib, sys
sys.path.insert(0, str(pathlib.Path(sys.argv[2]) / "tools" / "qa3"))
import source
marker = pathlib.Path(sys.argv[3])
real = source.MacosProcessApi.terminate
def fault(self, pid, identity, sig):
    try:
        survivor = int(marker.read_text())
    except (OSError, ValueError):
        survivor = -1
    if pid == survivor:
        raise OSError("fixture keeps an owned descendant alive")
    return real(self, pid, identity, sig)
source.MacosProcessApi.terminate = fault
command = json.loads(pathlib.Path(sys.argv[4]).read_text())
source._execute_owned(pathlib.Path(sys.argv[1]), float(sys.argv[5]), command)
"""


class MacosSafetyRegressionTests(unittest.TestCase):
    def start(self, procs):
        api = _FakeMacosApi(procs=procs, spawn_pid=200)
        supervisor = source.MacosSessionSupervisor(api, cleanup_interval=0.01)
        supervisor.uid = UID
        supervisor.prepare()
        supervisor.spawn([sys.executable, "-c", "pass"])
        return api, supervisor

    def test_session_reuse_between_scans_never_adopts_replacement(self):
        for replacement_leader in (True, False):
            with self.subTest(replacement_leader=replacement_leader):
                api, supervisor = self.start({301: (1, UID, 200, _later())})
                supervisor.track()
                # Explicit reap: after it, the held reservation cannot anchor
                # a scan, so a replacement session is never adopted.
                self.assertEqual(supervisor.process.poll(), 0)
                api.procs = {201: (1, UID, 200, _later(20))}
                if replacement_leader:
                    api.procs[200] = (1, UID, 200, _later(19))
                complete, errors = supervisor.cleanup()
                self.assertFalse(complete)
                self.assertTrue(errors)
                self.assertEqual(api.signalled, [])
                self.assertIn(201, api.procs)
                # A later empty table must not erase the uncertainty.
                api.procs.clear()
                self.assertFalse(supervisor.cleanup()[0])

    def test_unknown_first_snapshot_fails_without_signalling(self):
        api, supervisor = self.start({301: (1, UID, 200, _later())})
        # Explicit reap: afterwards no generation anchor is observed in the
        # scan, so an unknown member must fail closed without signalling.
        self.assertEqual(supervisor.process.poll(), 0)
        api.procs.pop(200, None)
        complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(errors)
        self.assertEqual(api.signalled, [])

    def test_known_owned_process_is_cleaned_after_session_continuity_loss(self):
        api, supervisor = self.start({301: (1, UID, 200, _later())})
        supervisor.track()
        # The leader slot is replaced while the Popen is still un-reaped: an
        # unreaped PID cannot change occupant, so this is the held-invariant
        # failure; the known owned pid is still cleaned, the new session is
        # never adopted and the verdict fails.
        api.procs = {
            301: (1, UID, 301, _later()),
            200: (1, UID, 200, _later(20)),
            201: (200, UID, 200, _later(21)),
        }
        complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(errors)
        self.assertEqual([pid for pid, _ in api.signalled], [301])
        self.assertIn(201, api.procs)

    def query(self, size, error=None):
        class Libproc:
            def proc_pidinfo(self, *_args):
                if error is not None:
                    ctypes.set_errno(error)
                return size
        with patch.object(source, "_mac_libproc", return_value=Libproc()):
            return source._mac_bsdinfo(301)

    def test_only_esrch_confirms_disappearance(self):
        self.assertIsNone(self.query(0, errno.ESRCH))
        for error in (errno.EPERM, errno.EIO, 0):
            with self.subTest(error=error), self.assertRaises(OSError):
                self.query(0, error)

    def test_partial_and_pid_mismatch_results_are_not_disappearance(self):
        for size in (1, ctypes.sizeof(source._MacProcBsdInfo)):
            with self.subTest(size=size), self.assertRaises(OSError):
                self.query(size, 0)

    def test_stale_errno_is_cleared_before_query(self):
        ctypes.set_errno(errno.ESRCH)
        with self.assertRaises(OSError) as raised:
            self.query(0)
        self.assertEqual(raised.exception.errno, errno.EIO)

    def test_session_permission_error_is_not_disappearance(self):
        api = source.MacosProcessApi.__new__(source.MacosProcessApi)
        with patch.object(source.os, "getsid", create=True,
                          side_effect=PermissionError(errno.EPERM, "fixture")):
            with self.assertRaises(PermissionError):
                api.session(301)
        with patch.object(source.os, "getsid", create=True,
                          side_effect=ProcessLookupError(errno.ESRCH, "fixture")):
            self.assertIsNone(api.session(301))

    def test_owned_inspection_denial_cannot_report_clean_cleanup(self):
        api, supervisor = self.start({301: (1, UID, 200, _later())})
        supervisor.track()
        class Libproc:
            def proc_pidinfo(self, *_args):
                ctypes.set_errno(errno.EPERM)
                return 0
        with patch.object(source, "_mac_libproc", return_value=Libproc()):
            # The bare object: the constructor loads libproc and is
            # darwin-only, while the inspection contract is host-portable.
            with patch.object(
                api, "is_alive",
                source.MacosProcessApi.__new__(source.MacosProcessApi).is_alive,
            ):
                complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(errors)
        self.assertIn(301, api.procs)

    def test_an_unclaimed_denied_pid_in_a_foreign_session_is_tolerated(self):
        api, supervisor = self.start({500: (1, UID, 500, _later())})
        api.denied.add(500)  # BSD-denied, but getsid reads a foreign session
        supervisor.track()
        self.assertNotIn(500, supervisor.owned)
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual(api.signalled, [])
        # Diagnostics are per scan: the next fresh list carries nothing.
        api.denied.clear()
        supervisor.track()
        self.assertEqual(api.unobservable, [])

    def test_a_denied_pid_in_the_owned_session_fails_without_adoption(self):
        api, supervisor = self.start({500: (1, UID, 200, _later())})
        api.denied.add(500)
        complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(
            any("belongs to the owned session" in e for e in errors)
        )
        self.assertNotIn(500, supervisor.owned)
        self.assertEqual(api.signalled, [])

    def test_a_denied_pid_with_an_unverifiable_session_fails(self):
        api, supervisor = self.start({500: (1, UID, 500, _later())})
        api.denied.add(500)
        api.session_denied.add(500)
        complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(
            any("session cannot be verified" in e for e in errors)
        )
        self.assertEqual(api.signalled, [])


    def test_a_denied_owned_pid_fails_and_is_not_signalled(self):
        api, supervisor = self.start({301: (1, UID, 200, _later())})
        supervisor.track()  # adopts 301 while it is describable
        self.assertIn(301, supervisor.owned)
        api.denied.add(301)  # then its inspection is denied
        complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(any("cannot be verified" in e for e in errors))
        self.assertEqual(api.signalled, [])
        self.assertIn(301, api.procs, "the denied owned pid was signalled")

    def test_a_known_owned_pid_absent_from_the_table_reads_as_gone(self):
        api, supervisor = self.start({301: (1, UID, 200, _later())})
        supervisor.track()
        del api.procs[301]
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual(api.signalled, [])

    def test_a_known_owned_absent_pid_denial_is_sticky(self):
        api, supervisor = self.start({301: (1, UID, 200, _later())})
        supervisor.track()
        del api.procs[301]
        api.ghost.add(301)  # listed nowhere, yet direct inspection is denied
        complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(any("cannot be verified" in e for e in errors))

    def test_the_first_inspection_error_wins_and_sticks(self):
        api, supervisor = self.start(
            {301: (1, UID, 200, _later()), 500: (1, UID, 500, _later())}
        )
        supervisor.track()
        api.denied.update((301, 500))  # owned 301 first, foreign 500 second
        api.session_denied.add(500)
        with self.assertRaises(OSError) as raised:
            supervisor.track()
        # First message wins: the owned denial, not the later session one.
        self.assertTrue(
            supervisor.inspection_error.startswith("owned process 301")
        )
        self.assertNotIn(
            "session cannot be verified", supervisor.inspection_error
        )
        self.assertIn("cannot be verified", str(raised.exception))
        # Uncertainty never expires, even after the denials clear.
        api.denied.clear()
        complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(any("cannot be verified" in e for e in errors))

    def test_a_denied_stray_after_the_session_dropped_is_tolerated(self):
        api, supervisor = self.start({})
        supervisor.track()
        self.assertEqual(supervisor.process.poll(), 0)
        supervisor.track()  # the reaped, empty session is no longer matched
        self.assertIsNone(supervisor.session)
        api.procs[500] = (1, UID, 500, _later())
        api.denied.add(500)
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual(api.signalled, [])

    def test_a_denied_owned_pid_with_a_new_birth_falls_to_the_session_check(self):
        api, supervisor = self.start({301: (1, UID, 200, _later())})
        supervisor.track()
        # The scan denies 301, but a direct inspection reads a new birth:
        # the stale claim is dropped and the occupant is judged afresh.
        api.denied.add(301)
        api.rebound[301] = _later(9)
        api.procs[301] = (1, UID, 200, _later(9))
        complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(
            any("belongs to the owned session" in e for e in errors)
        )
        self.assertNotIn(301, supervisor.owned)
        self.assertEqual(api.signalled, [])

    def test_a_denied_owned_pid_with_a_foreign_birth_is_a_fresh_nonmember(self):
        api, supervisor = self.start({301: (1, UID, 200, _later())})
        supervisor.track()
        api.denied.add(301)
        api.rebound[301] = _later(9)
        api.procs[301] = (1, UID, 301, _later(9))
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertNotIn(301, supervisor.owned)
        self.assertEqual(api.signalled, [])

    def test_a_denied_owned_pid_with_its_own_birth_keeps_the_claim(self):
        api, supervisor = self.start({301: (1, UID, 200, _later())})
        supervisor.track()
        api.denied.add(301)
        api.rebound[301] = _later()  # same birth: the claim stands
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual([pid for pid, _sig in api.signalled], [301])


class _ScriptedLibproc:
    """Scripted libproc: records proc_listpids calls, answers per script.

    Each answer is \`(value, errnum, pids)\`: a negative \`value\` fails with
    \`errnum\`; \`value\` zero fails closed; \`value\` \`None\` answers with the
    byte count of \`pids\`; \`pids\` "exact" answers with the byte size the
    caller offered (an exactly-full buffer); a plain non-negative value
    answers raw.
    """

    def __init__(self, answers, info=None, denied=()):
        self.answers = list(answers)
        self.info = dict(info or {})
        self.denied = set(denied)
        self.list_calls = []
        self.info_calls = []

    def proc_listpids(self, kind, typeinfo, buffer, size):
        self.list_calls.append((kind, typeinfo, size))
        if not self.answers:
            raise AssertionError("unexpected proc_listpids call")
        value, errnum, pids = self.answers.pop(0)
        if pids == "exact":
            # An exactly-full buffer: answer with the offered byte size.
            if buffer is None:
                raise AssertionError("an exact answer needs a buffer")
            return size
        if value is None:
            # A pid fill: answer with the byte count the pids need.
            if buffer is None:
                raise AssertionError("a pid fill needs a buffer")
            for index, pid in enumerate(pids):
                buffer[index] = pid
            return 4 * len(pids)
        # A raw answer: zero or negative fails closed, positive is a byte
        # count that fills nothing (used for partial-slot answers).
        if value <= 0:
            ctypes.set_errno(errnum)
        return value

    def proc_pidinfo(self, pid, _type, _arg, buffer, _size):
        self.info_calls.append(pid)
        if pid in self.denied:
            ctypes.set_errno(errno.EPERM)
            return 0
        fields = self.info.get(pid)
        if fields is None:
            ctypes.set_errno(errno.ESRCH)
            return 0
        native = source._MacProcBsdInfo()
        (ppid, uid, status, identity) = fields
        native.pbi_pid = pid
        native.pbi_ppid = ppid
        native.pbi_uid = uid
        native.pbi_status = status
        native.pbi_start_tvsec = identity[0]
        native.pbi_start_tvusec = identity[1]
        ctypes.set_errno(0)
        ctypes.memmove(buffer, ctypes.byref(native), ctypes.sizeof(native))
        return ctypes.sizeof(native)


class MacosUidListingTests(unittest.TestCase):
    """The uid-scoped listing behind MacosProcessApi.process_table."""

    def _api(self):
        # The real constructor loads libproc and is darwin-only; the listing
        # and its table logic are host contracts, so build the bare object.
        return source.MacosProcessApi.__new__(source.MacosProcessApi)

    def _listed(self, lib):
        with patch.object(source, "_mac_libproc", return_value=lib):
            return source._mac_uid_pids(UID)

    def _table(self, lib):
        api = self._api()
        with patch.object(source, "_mac_libproc", return_value=lib), \
             patch.object(source.os, "getuid", create=True, return_value=UID):
            table = api.process_table()
        return api, table

    def test_uid_pids_probe_and_fill_use_uid_only_bytes(self):
        lib = _ScriptedLibproc(
            answers=[(12, 0, None), (None, 0, [11, 22, 33])],
        )
        self.assertEqual(self._listed(lib), [11, 22, 33])
        # The probe carries no buffer; the fill is byte-sized, never slots.
        self.assertEqual(
            lib.list_calls,
            [
                (source._MAC_PROC_UID_ONLY, UID, 0),
                (source._MAC_PROC_UID_ONLY, UID, (12 // 4 + 256) * 4),
            ],
        )
        # The uid-only listing has no global all-pids fallback.
        self.assertFalse(hasattr(lib, "proc_listallpids"))

    def test_a_failed_listing_raises_without_falling_back(self):
        lib = _ScriptedLibproc(answers=[(-1, errno.EIO, None)])
        with self.assertRaises(OSError):
            self._listed(lib)
        self.assertEqual(len(lib.list_calls), 1)  # no retry, no fallback

    def test_a_zero_listing_fails_closed_even_with_a_wrapper_errno(self):
        lib = _ScriptedLibproc(answers=[(0, errno.EPERM, None)])
        with self.assertRaises(OSError):
            self._listed(lib)
        self.assertEqual(len(lib.list_calls), 1)  # no fill is attempted

    def test_an_absurd_hint_fails_before_the_big_allocation(self):
        lib = _ScriptedLibproc(answers=[(1 << 30, 0, None)])
        with self.assertRaises(OSError):
            self._listed(lib)
        self.assertEqual(len(lib.list_calls), 1)  # nothing was allocated

    def test_a_failed_fill_raises_without_falling_back(self):
        lib = _ScriptedLibproc(answers=[(12, 0, None), (-1, errno.EPERM, None)])
        with self.assertRaises(OSError):
            self._listed(lib)

    def test_a_partial_pid_slot_is_never_truncated(self):
        for stage in ("probe", "fill"):
            with self.subTest(stage=stage):
                if stage == "probe":
                    lib = _ScriptedLibproc(answers=[(6, 0, None)])
                else:
                    lib = _ScriptedLibproc(
                        answers=[(12, 0, None), (6, 0, None)]
                    )
                with self.assertRaises(OSError) as raised:
                    self._listed(lib)
                self.assertIn("partial PID slot", str(raised.exception))

    def test_a_persistently_exact_full_listing_fails_closed(self):
        lib = _ScriptedLibproc(
            answers=[(400, 0, None)] + [(0, 0, "exact")] * 6
        )
        with self.assertRaises(OSError) as raised:
            self._listed(lib)
        self.assertIn("kept growing", str(raised.exception))
        # Bounded growth: the probe plus exactly four fill attempts.
        self.assertEqual(len(lib.list_calls), 5)

    def test_process_table_reports_denied_pids_as_unobservable(self):
        lib = _ScriptedLibproc(
            answers=[(16, 0, None), (None, 0, [301, 500, 600])],
            info={
                301: (1, UID, 0, _later()),
                600: (2, UID + 1, 0, _later(2)),
            },
            denied={500},
        )
        api, table = self._table(lib)
        self.assertEqual(api.unobservable, [(500, errno.EPERM)])
        self.assertEqual(table[301], (1, UID, 0, _later()))
        self.assertEqual(table[600], (2, UID + 1, 0, _later(2)))
        self.assertNotIn(500, table)

    def test_an_esrch_listed_pid_is_gone_not_unobservable(self):
        lib = _ScriptedLibproc(
            answers=[(16, 0, None), (None, 0, [301, 500])],
            info={301: (1, UID, 0, _later())},  # 500 answers ESRCH
        )
        api, table = self._table(lib)
        self.assertNotIn(500, table)
        self.assertEqual(api.unobservable, [])

    def test_the_unobservable_list_is_replaced_every_scan(self):
        lib = _ScriptedLibproc(
            answers=[
                (16, 0, None), (None, 0, [301, 500]),
                (12, 0, None), (None, 0, [301]),
            ],
            info={301: (1, UID, 0, _later())},
            denied={500},
        )
        with patch.object(source, "_mac_libproc", return_value=lib), \
             patch.object(source.os, "getuid", create=True, return_value=UID):
            api = self._api()
            first = api.process_table()
            self.assertEqual(api.unobservable, [(500, errno.EPERM)])
            second = api.process_table()
        self.assertEqual(
            api.unobservable, [], "a stale denial leaked into a fresh scan"
        )
        self.assertEqual(set(first), {301})
        self.assertEqual(set(second), {301})

    def test_a_failed_scan_still_resets_the_unobservable_list(self):
        lib = _ScriptedLibproc(
            answers=[(16, 0, None), (None, 0, [301, 500])],
            info={301: (1, UID, 0, _later())},
            denied={500},
        )
        with patch.object(source, "_mac_libproc", return_value=lib), \
             patch.object(source.os, "getuid", create=True, return_value=UID):
            api = self._api()
            api.process_table()
            self.assertEqual(api.unobservable, [(500, errno.EPERM)])
            lib.answers = [(-1, errno.EIO, None)]
            with self.assertRaises(OSError):
                api.process_table()
        # The reset happens at entry, before any listing failure.
        self.assertEqual(api.unobservable, [])

    def test_more_unobservable_pids_than_the_cap_raises_without_truncating(self):
        pids = [700_000 + offset for offset in range(1, 301)]
        lib = _ScriptedLibproc(
            answers=[(1400, 0, None), (None, 0, pids)],
            denied=set(pids),
        )
        with patch.object(source, "_mac_libproc", return_value=lib), \
             patch.object(source.os, "getuid", create=True, return_value=UID):
            api = self._api()
            with self.assertRaises(OSError) as raised:
                api.process_table()
        self.assertIn(
            "unobservable process list exceeded", str(raised.exception)
        )
        # The retained list stops at the cap; the preview names the first 8.
        self.assertEqual(len(api.unobservable), 256)
        self.assertIn(str(pids[0]), str(raised.exception))
        self.assertNotIn(
            str(pids[source._MAC_UNOBSERVABLE_PREVIEW]),
            str(raised.exception),
        )


@unittest.skipUnless(MACOS, "requires macOS libproc")
class MacosSessionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def _command(self, name, argv):
        path = self.root / name
        path.write_text(json.dumps(argv))
        return path

    def _worker(self, script, arguments, timeout=90):
        result = self.root / "supervisor-result.json"
        completed = subprocess.run(
            [
                sys.executable,
                "-I",
                "-B",
                "-c",
                script,
                str(result),
                str(source.ROOT),
                *arguments,
            ],
            cwd=str(source.ROOT),
            env=os.environ.copy(),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
        )
        if not result.is_file():
            self.fail(
                f"the supervisor produced no result: {completed.stdout!r}"
            )
        return json.loads(result.read_text())

    def _kill_later(self, marker):
        def kill():
            try:
                os.kill(int(marker.read_text()), signal.SIGKILL)
            except (OSError, ValueError):
                pass

        self.addCleanup(kill)

    def test_real_api_describes_this_process(self):
        api = source.MacosProcessApi()
        api.check()
        table = api.process_table()
        self.assertIn(os.getpid(), table)
        parent, uid, _status, identity = table[os.getpid()]
        self.assertEqual(parent, os.getppid())
        self.assertEqual(uid, os.getuid())
        self.assertEqual(identity, api.identity(os.getpid()))
        self.assertLessEqual(identity, api.now())
        self.assertEqual(api.session(os.getpid()), os.getsid(0))
        self.assertTrue(api.is_alive(os.getpid(), identity))
        self.assertFalse(api.is_alive(os.getpid(), (0, 0)))

    def test_real_supervisor_collects_an_orphan_and_keeps_an_unrelated_process(self):
        marker = self.root / "orphan-pid"
        self._kill_later(marker)
        unrelated = subprocess.Popen(
            [sys.executable, "-c", "import time; time.sleep(30)"]
        )
        self.addCleanup(unrelated.wait, 5)
        self.addCleanup(unrelated.kill)
        command = self._command("command.json", _orphan_command(marker))
        result = self._worker(WORKER, [str(command), "20"])
        self.assertEqual(result["exitCode"], 0, result)
        self.assertTrue(result["ownedCleanupComplete"], result)
        self.assertEqual(result["cleanupErrors"], [])
        self.assertEqual(result["verdict"], "PASS")
        self.assertTrue(marker.is_file(), "the orphan fixture never ran")
        self.assertTrue(
            _wait_dead(int(marker.read_text())),
            "an orphaned owned descendant survived cleanup",
        )
        self.assertIsNone(unrelated.poll(), "an unrelated process was stopped")

    def test_real_supervisor_collects_a_descendant_that_left_the_session(self):
        marker = self.root / "escaped-pid"
        self._kill_later(marker)
        command = self._command(
            "command.json", _orphan_command(marker, leave_session=True)
        )
        result = self._worker(WORKER, [str(command), "20"])
        self.assertEqual(result["verdict"], "PASS", result)
        self.assertTrue(marker.is_file(), "the escaping fixture never ran")
        escaped = int(marker.read_text())
        self.assertTrue(
            _wait_dead(escaped),
            "a descendant that called setsid survived cleanup",
        )

    def test_real_supervisor_reports_cleanup_failure_and_fails_the_verdict(self):
        marker = self.root / "survivor-pid"
        self._kill_later(marker)
        command = self._command("command.json", _orphan_command(marker))
        result = self._worker(
            WORKER_WITH_CLEANUP_FAULT,
            [str(marker), str(command), "20"],
        )
        self.assertTrue(marker.is_file(), "the survivor fixture never ran")
        self.assertFalse(result["ownedCleanupComplete"], result)
        self.assertIn(
            "fixture keeps an owned descendant alive",
            result["cleanupErrors"][0],
        )
        self.assertEqual(result["verdict"], "FAIL")
        self.assertIsNotNone(source.supervisor_failure(result))
        survivor = int(marker.read_text())
        self.assertTrue(
            source.MacosProcessApi().is_alive(survivor),
            "the survivor was stopped although its signal failed",
        )

    def test_real_unanchored_orphan_fails_closed_without_adoption(self):
        marker = self.root / "unanchored-orphan-pid"
        self._kill_later(marker)
        supervisor = source.MacosSessionSupervisor(cleanup_interval=0.1)
        supervisor.prepare()
        process = supervisor.spawn(_orphan_command(marker))
        # Deliberately violate scan-before-reap: this must not justify a kill.
        self.assertEqual(process.wait(timeout=20), 0)
        orphan = int(marker.read_text())
        complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(errors)
        self.assertNotIn(orphan, supervisor.owned)
        self.assertTrue(supervisor.api.is_alive(orphan))

    def test_real_session_rule_alone_collects_an_orphan_and_spares_a_later_process(self):
        marker = self.root / "session-orphan-pid"
        self._kill_later(marker)
        supervisor = source.MacosSessionSupervisor(cleanup_interval=1.0)
        supervisor.prepare()
        process = supervisor.spawn(_orphan_command(marker))
        # Born after the spawn, same user, another session: only the session
        # check itself tells it apart from the orphan.
        unrelated = subprocess.Popen(
            [sys.executable, "-c", "import time; time.sleep(30)"]
        )
        self.addCleanup(unrelated.wait, 5)
        self.addCleanup(unrelated.kill)
        # No tracking while the command runs: the ancestry rule never sees
        # the orphan. Keep the exited leader unreaped until the first scan,
        # exactly as run_owned_supervisor does (track before Popen.poll).
        deadline = time.monotonic() + 20
        while supervisor.api.is_alive(process.pid, supervisor.owned[process.pid]):
            self.assertLess(time.monotonic(), deadline, 'command did not exit')
            time.sleep(0.02)
        # Native observation: an un-reaped zombie may have no BSD description
        # at all (proc_pidinfo answers ESRCH), so BSD info is optional; when
        # it is readable it must be a zombie. The required evidence is the
        # BSD-based exit plus the held reservation, never kill(0) alone.
        leader_info = source._mac_bsdinfo(process.pid)
        if leader_info is not None:
            self.assertEqual(leader_info[2], source._MAC_SZOMB)
        self.assertFalse(
            supervisor.api.is_alive(process.pid, supervisor.owned[process.pid]),
            "the exited leader must not read as alive",
        )
        self.assertIsNone(process.returncode, "the leader was reaped early")
        os.kill(process.pid, 0)  # the held reservation still owns the PID
        self.assertEqual(set(supervisor.owned), {process.pid})
        orphan = int(marker.read_text())
        orphan_info = source._mac_bsdinfo(orphan)
        self.assertIsNotNone(orphan_info)
        self.assertNotEqual(
            orphan_info[0], process.pid, 'fixture child is not orphaned'
        )
        supervisor.track()
        self.assertIn(orphan, supervisor.owned)
        self.assertEqual(process.wait(timeout=20), 0)
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual(errors, [])
        self.assertIn(orphan, supervisor.owned)
        self.assertTrue(_wait_dead(orphan), "the session orphan survived")
        self.assertIsNone(unrelated.poll(), "a later unrelated process was stopped")

    def test_source_plan_host_remains_linux_only(self):
        output = self.root / "linux-host"
        self.assertEqual(source.run_source(output, "0" * 40), 1)
        receipt = source.read_json(output / "source-receipt.json")
        self.assertEqual(receipt["sourceVerdict"], "FAIL")
        self.assertIn("Linux x86_64", receipt["error"])


if __name__ == "__main__":
    unittest.main()

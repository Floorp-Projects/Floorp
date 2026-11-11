# SPDX-License-Identifier: MPL-2.0

"""macOS session port of the QA3 owned-process supervisor contract.

The Linux supervisor (``test_source.py``) and the Windows Job Object port
(``test_source_windows.py``) promise three things: collect every descendant,
never stop unrelated processes, and treat incomplete cleanup as a fail
verdict. These tests pin the same three promises for the macOS session
supervisor. The contract classes run everywhere against an in-memory process
API, so the decision logic is covered on any host; ``MacosSessionTests`` runs
the real libproc path on macOS.
"""

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
    def __init__(self, api, pid):
        self._api = api
        self.pid = pid
        self.signals = []

    @property
    def returncode(self):
        return self._api.exit_codes.get(self.pid)

    def poll(self):
        return self.returncode

    def wait(self, timeout=None):
        return self.returncode

    def send_signal(self, sig):
        self.signals.append(sig)


class _FakeMacosApi:
    """In-memory stand-in for the libproc/getsid primitives.

    ``procs`` maps pid to ``(ppid, uid, session, identity)``. ``spawn_pid`` is
    the command the supervisor starts; it leads its own session and is
    recorded as already exited, like a command that finished before cleanup.
    """

    def __init__(self, procs=None, spawn_pid=900000, now=BORN):
        self.procs = dict(procs or {})
        self.exit_codes = {}
        self.spawn_pid = spawn_pid
        self.clock = now
        self.signalled = []
        self.checked = False

    def check(self):
        self.checked = True

    def now(self):
        return self.clock

    def spawn(self, argv):
        self.procs[self.spawn_pid] = (os.getpid(), UID, self.spawn_pid, BORN)
        process = _FakeProcess(self, self.spawn_pid)
        self.exit_codes[self.spawn_pid] = 0
        del self.procs[self.spawn_pid]
        return process

    def process_table(self):
        return {
            pid: (ppid, uid, 0, identity)
            for pid, (ppid, uid, _session, identity) in self.procs.items()
        }

    def identity(self, pid):
        entry = self.procs.get(pid)
        return None if entry is None else entry[3]

    def session(self, pid):
        entry = self.procs.get(pid)
        return None if entry is None else entry[2]

    def is_alive(self, pid, identity=None):
        entry = self.procs.get(pid)
        if entry is None:
            return False
        return identity is None or entry[3] == identity

    def terminate(self, pid, identity, sig):
        entry = self.procs.get(pid)
        if entry is None or entry[3] != identity:
            return
        self.signalled.append((pid, sig))
        del self.procs[pid]


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


def _wait_dead(pid, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not source.MacosProcessApi().is_alive(pid):
            return True
        time.sleep(0.05)
    return False


def _orphan_command(marker, leave_session=False):
    """A command whose child outlives it, reparented to launchd.

    With ``leave_session`` the child first waits to be seen, then calls
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
            self.fail(f"the supervisor produced no result: {completed.stdout!r}")
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
        # the orphan, which is reparented to launchd once the command exits.
        self.assertEqual(process.wait(timeout=20), 0)
        self.assertEqual(set(supervisor.owned), {process.pid})
        orphan = int(marker.read_text())
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

# SPDX-License-Identifier: MPL-2.0

"""Windows Job Object port of the QA3 owned-process supervisor contract.

The Linux supervisor in ``test_source.py`` promises three things: collect every
descendant, never stop unrelated processes, and treat incomplete cleanup as a
fail verdict. These tests pin the same three promises for the Windows Job
Object implementation. The contract classes run everywhere against an
in-memory Job Object API so the decision logic is covered on any host; the
``WindowsJobObjectTests`` class runs the real Win32 path on ``windows-latest``.
"""

import json
import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

import source

WINDOWS = os.name == "nt"


class _FakeProcess:
    def __init__(self, api, pid):
        self._api = api
        self.pid = pid

    @property
    def returncode(self):
        return self._api.exit_codes.get(self.pid)

    def poll(self):
        return self.returncode

    def wait(self, timeout=None):
        return self.returncode


class _FakeJobObjectApi:
    """In-memory stand-in for the Windows Job Object primitives.

    ``tree`` maps pid to parent pid, ``members`` is the job process list, and
    ``spawn_pid`` is the command the supervisor starts. Spawned commands are
    recorded as already exited, matching a command that finished before cleanup.
    """

    def __init__(self, tree=None, members=None, spawn_pid=900000):
        self.table = dict(tree or {})
        self.members = set(members or ())
        self.identities = {pid: 1000 + pid for pid in self.table}
        self.exit_codes = {}
        self.spawn_pid = spawn_pid
        self.spawned = None
        self.terminated = []
        self.calls = []
        self.job = None
        self.flags = 0

    def create_job(self):
        self.calls.append("create_job")
        self.job = 1

    def enable_kill_on_job_close(self):
        self.calls.append("enable_kill_on_job_close")
        self.flags |= source.WINDOWS_JOB_KILL_ON_JOB_CLOSE

    def limit_flags(self):
        return self.flags

    def assign_current_process(self):
        self.calls.append("assign_current_process")

    def spawn(self, argv):
        self.calls.append(("spawn", tuple(argv)))
        self.spawned = self.spawn_pid
        self.identities[self.spawn_pid] = 1000 + self.spawn_pid
        self.members.add(self.spawn_pid)
        self.table[self.spawn_pid] = os.getpid()
        self.exit_codes[self.spawn_pid] = 0
        del self.table[self.spawn_pid]
        return _FakeProcess(self, self.spawn_pid)

    def process_table(self):
        return dict(self.table)

    def identity(self, pid):
        return self.identities.get(pid)

    def is_alive(self, pid, identity=None):
        if pid not in self.table:
            return False
        return identity is None or self.identities.get(pid) == identity

    def job_members(self):
        return {pid for pid in self.members if pid in self.table}

    def terminate(self, pid, identity=None, allow_non_member=False):
        self.calls.append(("terminate", pid))
        if pid not in self.table:
            return True
        if identity is not None and self.identities.get(pid) != identity:
            return True
        if pid not in self.members and not allow_non_member:
            return False
        del self.table[pid]
        self.exit_codes[pid] = 1
        self.terminated.append(pid)
        return True

    def close_job(self):
        self.calls.append("close_job")
        self.job = None


class WindowsSupervisorContractTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def _supervisor(self, api):
        return source.WindowsJobSupervisor(api, cleanup_interval=0.05)

    def test_supervisor_kind_selects_the_job_object_on_windows(self):
        self.assertEqual(source.supervisor_kind("nt"), "windows")
        self.assertEqual(source.supervisor_kind("posix", "linux"), "linux")
        self.assertEqual(source.supervisor_kind("posix", "darwin"), "macos")
        if WINDOWS:
            self.assertEqual(source.supervisor_kind(), "windows")
        if sys.platform == "linux":
            self.assertEqual(source.supervisor_kind(), "linux")
            self.assertIsInstance(
                source.owned_supervisor(), source.PosixSubreaperSupervisor
            )
            with self.assertRaises(OSError):
                source.WindowsJobObjectApi()

    def test_job_object_gets_kill_on_close_before_the_command_is_spawned(self):
        api = _FakeJobObjectApi()
        supervisor = self._supervisor(api)
        supervisor.prepare()
        supervisor.spawn([sys.executable, "-c", "pass"])
        self.assertEqual(
            api.calls[:3],
            [
                "create_job",
                "enable_kill_on_job_close",
                "assign_current_process",
            ],
        )
        self.assertEqual(api.calls[3][0], "spawn")
        self.assertTrue(api.flags & source.WINDOWS_JOB_KILL_ON_JOB_CLOSE)
        self.assertNotIn("close_job", api.calls)

    def test_every_descendant_is_collected_and_unrelated_processes_survive(self):
        api = _FakeJobObjectApi(
            tree={200: os.getpid(), 300: 200, 301: 300, 400: 1},
            members={200, 300, 301},
            spawn_pid=200,
        )
        supervisor = self._supervisor(api)
        supervisor.prepare()
        supervisor.spawn([sys.executable, "-c", "pass"])
        supervisor.track()
        self.assertEqual(set(supervisor.owned), {200, 300, 301})
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual(errors, [])
        self.assertEqual(sorted(api.terminated), [300, 301])
        self.assertIn(400, api.table, "an unrelated process was stopped")
        self.assertNotIn(400, api.terminated)

    def test_job_membership_collects_a_descendant_whose_parent_is_gone(self):
        api = _FakeJobObjectApi(
            tree={200: os.getpid(), 301: 4242},
            members={200, 301},
            spawn_pid=200,
        )
        supervisor = self._supervisor(api)
        supervisor.prepare()
        supervisor.spawn([sys.executable, "-c", "pass"])
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual(api.terminated, [301])

    def test_reused_process_identifier_is_not_treated_as_an_owned_descendant(self):
        api = _FakeJobObjectApi(
            tree={200: os.getpid(), 301: 200},
            members={200, 301},
            spawn_pid=200,
        )
        supervisor = self._supervisor(api)
        supervisor.prepare()
        supervisor.spawn([sys.executable, "-c", "pass"])
        supervisor.track()
        api.identities[301] = 999999
        complete, errors = supervisor.cleanup()
        self.assertTrue(complete, errors)
        self.assertEqual(api.terminated, [])
        self.assertIn(301, api.table)

    def test_owned_descendant_that_survives_cleanup_fails_the_verdict(self):
        class SurvivingApi(_FakeJobObjectApi):
            def terminate(self, pid, identity=None, allow_non_member=False):
                if pid == 301:
                    raise OSError("fixture keeps an owned descendant alive")
                return super().terminate(pid, identity, allow_non_member)

        api = SurvivingApi(
            tree={200: os.getpid(), 301: 200},
            members={200, 301},
            spawn_pid=200,
        )
        supervisor = self._supervisor(api)
        with (
            patch.object(source, "owned_supervisor", return_value=supervisor),
            patch.object(source, "_install_stop_handlers", lambda stop: None),
        ):
            result = source.run_owned_supervisor(
                5.0, [sys.executable, "-c", "pass"]
            )
        self.assertEqual(result["exitCode"], 0)
        self.assertFalse(result["ownedCleanupComplete"])
        self.assertTrue(result["cleanupErrors"])
        self.assertIn(
            "fixture keeps an owned descendant alive",
            result["cleanupErrors"][0],
        )
        self.assertEqual(result["verdict"], "FAIL")
        self.assertIsNotNone(source.supervisor_failure(result))
        self.assertIn(301, api.table, "the surviving descendant was not verified")
        self.assertNotIn("close_job", api.calls)

    def test_cleanup_failure_is_never_a_pass_verdict(self):
        clean = {
            "exitCode": 0,
            "error": None,
            "cleanupErrors": [],
            "ownedCleanupComplete": True,
        }
        self.assertEqual(source.supervisor_verdict(clean), "PASS")
        self.assertIsNone(source.supervisor_failure(clean))
        for change in (
            {"ownedCleanupComplete": False},
            {"cleanupErrors": ["OSError: survivor"]},
            {"error": "source command timed out"},
            {"exitCode": None},
            {"exitCode": 3},
        ):
            with self.subTest(change=change):
                self.assertEqual(
                    source.supervisor_verdict({**clean, **change}), "FAIL"
                )
        for change in (
            {"ownedCleanupComplete": False},
            {"cleanupErrors": ["OSError: survivor"]},
        ):
            with self.subTest(change=change):
                self.assertIn(
                    "cleanup",
                    source.supervisor_failure({**clean, **change}),
                )

    def test_unprepared_job_object_cannot_report_a_clean_result(self):
        api = _FakeJobObjectApi(tree={200: os.getpid()}, spawn_pid=200)
        supervisor = self._supervisor(api)
        supervisor.process = _FakeProcess(api, 200)
        complete, errors = supervisor.cleanup()
        self.assertFalse(complete)
        self.assertTrue(errors)

    def test_execute_fails_closed_when_the_supervisor_reports_bad_cleanup(self):
        class FakeChild:
            returncode = 0

            def wait(self, timeout=None):
                return 0

        status = {
            "exitCode": 0,
            "error": None,
            "cleanupErrors": [],
            "ownedCleanupComplete": True,
            "verdict": "PASS",
        }

        def fake_popen(_argv, **kwargs):
            os.write(kwargs["pass_fds"][0], json.dumps(payload).encode())
            return FakeChild()

        with patch.object(source.subprocess, "Popen", fake_popen):
            payload = status
            self.assertEqual(
                source.execute(
                    [sys.executable, "-c", "pass"],
                    self.root / "clean.log",
                    self.root,
                ),
                0,
            )
            for index, (change, expected) in enumerate(
                (
                    ({"ownedCleanupComplete": False, "verdict": "FAIL"}, "cleanup"),
                    (
                        {
                            "cleanupErrors": ["OSError: owned descendant survived"],
                            "verdict": "FAIL",
                        },
                        "cleanup",
                    ),
                    ({"verdict": "FAIL"}, "untyped"),
                )
            ):
                payload = {**status, **change}
                with self.subTest(change=change):
                    with self.assertRaisesRegex(ValueError, expected):
                        source.execute(
                            [sys.executable, "-c", "pass"],
                            self.root / f"cleanup-fault-{index}.log",
                            self.root,
                        )


def _wait_dead(pid, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not source._win_is_alive(pid):
            return True
        time.sleep(0.05)
    return False


def _descendant_command(marker):
    child = (
        "import os,pathlib,time\n"
        f"pathlib.Path({str(marker)!r}).write_text(str(os.getpid()))\n"
        "time.sleep(10)\n"
    )
    parent = (
        "import pathlib,subprocess,sys,time\n"
        f"marker = pathlib.Path({str(marker)!r})\n"
        f"subprocess.Popen([sys.executable,'-I','-c',{child!r}])\n"
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
real = source._win_terminate
def fault(job, pid, identity=None, allow_non_member=False):
    try:
        survivor = int(marker.read_text())
    except (OSError, ValueError):
        survivor = -1
    if pid == survivor:
        raise OSError("fixture keeps an owned descendant alive")
    return real(job, pid, identity, allow_non_member)
source._win_terminate = fault
command = json.loads(pathlib.Path(sys.argv[4]).read_text())
source._execute_owned(pathlib.Path(sys.argv[1]), float(sys.argv[5]), command)
"""


@unittest.skipUnless(WINDOWS, "requires Windows Job Object support")
class WindowsJobObjectTests(unittest.TestCase):
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

    def test_real_job_collects_detached_descendant_and_keeps_unrelated_process(self):
        marker = self.root / "descendant-pid"
        unrelated = subprocess.Popen(
            [sys.executable, "-c", "import time; time.sleep(30)"]
        )
        self.addCleanup(unrelated.wait, 5)
        self.addCleanup(unrelated.kill)
        command = self._command("command.json", _descendant_command(marker))
        result = self._worker(WORKER, [str(command), "20"])
        self.assertEqual(result["exitCode"], 0, result)
        self.assertTrue(result["ownedCleanupComplete"], result)
        self.assertEqual(result["cleanupErrors"], [])
        self.assertEqual(result["verdict"], "PASS")
        self.assertTrue(marker.is_file(), "the descendant fixture never ran")
        self.assertTrue(
            _wait_dead(int(marker.read_text())),
            "a detached owned descendant survived cleanup",
        )
        self.assertIsNone(unrelated.poll(), "an unrelated process was stopped")

    def test_real_job_reports_cleanup_failure_and_fails_the_verdict(self):
        marker = self.root / "survivor-pid"
        command = self._command("command.json", _descendant_command(marker))
        result = self._worker(
            WORKER_WITH_CLEANUP_FAULT,
            [str(marker), str(command), "20"],
        )
        self.assertTrue(marker.is_file(), "the survivor fixture never ran")
        survivor = int(marker.read_text())
        self.assertFalse(result["ownedCleanupComplete"], result)
        self.assertTrue(result["cleanupErrors"], result)
        self.assertIn(
            "fixture keeps an owned descendant alive",
            result["cleanupErrors"][0],
        )
        self.assertEqual(result["verdict"], "FAIL")
        self.assertIsNotNone(source.supervisor_failure(result))
        self.assertTrue(
            _wait_dead(survivor),
            "kill-on-close did not reap the surviving descendant",
        )

    def test_real_job_object_retains_kill_on_close(self):
        api = source.WindowsJobObjectApi()
        try:
            api.create_job()
            api.enable_kill_on_job_close()
            self.assertTrue(
                api.limit_flags() & source.WINDOWS_JOB_KILL_ON_JOB_CLOSE
            )
            self.assertEqual(api.job_members(), set())
            self.assertIn(os.getpid(), api.process_table())
        finally:
            api.close_job()

    def test_source_plan_host_remains_linux_only(self):
        output = self.root / "linux-host"
        self.assertEqual(source.run_source(output, "0" * 40), 1)
        receipt = source.read_json(output / "source-receipt.json")
        self.assertEqual(receipt["sourceVerdict"], "FAIL")
        self.assertIn("Linux x86_64", receipt["error"])


if __name__ == "__main__":
    unittest.main()

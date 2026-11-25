#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Platform-independent coverage of the Windows HostProcess branches.

Windows has no SIGKILL, subprocess terminate()/kill() both end the child with
TerminateProcess(handle, 1), and the launcher process stays alive as the parent
of the Marionette browser. These tests mock the process layer, so they run on
any OS.
"""

import importlib.util
from pathlib import Path
import signal
import subprocess
import unittest
from unittest.mock import Mock, patch


spec = importlib.util.spec_from_file_location("app_shim_runtime_windows", Path(__file__).with_name("test-runtime.py"))
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)

LAUNCHER_PID = 4100
BROWSER_PID = 4200
START = "2026-11-07T06:00:00.0000000Z"


class WindowsHostProcessTests(unittest.TestCase):
    def setUp(self):
        windows = patch.object(runtime, "IS_WINDOWS", True)
        windows.start()
        self.addCleanup(windows.stop)
        self.profile = Path("C:/Temp/private unique/profile")
        self.binary = Path("C:/Floorp QA/floorp.exe")
        with patch.object(runtime.subprocess, "Popen") as launch:
            launch.return_value = Mock(pid=LAUNCHER_PID)
            self.host = runtime.HostProcess(self.binary, self.binary, self.profile,
                                            self.profile.parent, Mock())
        self.command_line = subprocess.list2cmdline(self.host.command)

    def rows(self, *rows):
        return patch.object(runtime.HostProcess, "_process_rows", return_value=list(rows))

    def test_identity_uses_windows_quoted_command_line(self):
        self.assertIn('"C:\\Floorp QA\\floorp.exe"', self.command_line)
        with self.rows((BROWSER_PID, START, self.command_line)):
            self.assertEqual(self.host._matching_identity(BROWSER_PID),
                             (BROWSER_PID, START, self.command_line))
        with self.rows((BROWSER_PID, START, " ".join(self.host.command))):
            self.assertIsNone(self.host._matching_identity(BROWSER_PID))

    def session(self, pid):
        return {"capabilities": {"moz:processID": pid, "moz:profile": str(self.profile)}}

    def test_browser_child_of_launcher_is_adopted(self):
        with self.rows((BROWSER_PID, START, self.command_line)), \
                patch.object(runtime.HostProcess, "_windows_parent_pid", return_value=LAUNCHER_PID):
            self.host.verify_session(self.session(BROWSER_PID))
        self.assertTrue(self.host.verified)
        self.assertEqual(self.host.pid, BROWSER_PID)

    def test_browser_not_child_of_launcher_is_rejected(self):
        with self.rows((BROWSER_PID, START, self.command_line)), \
                patch.object(runtime.HostProcess, "_windows_parent_pid", return_value=9999):
            with self.assertRaisesRegex(RuntimeError, "differs from the launched child"):
                self.host.verify_session(self.session(BROWSER_PID))
        self.assertFalse(self.host.verified)

    def test_kill_uses_terminate_process_without_sigkill(self):
        with patch.object(runtime.signal, "SIGKILL", None, create=True):
            self.host.kill()
        self.host.process.kill.assert_called_once_with()
        self.host.process.send_signal.assert_not_called()
        self.assertEqual(self.host.requested_signal, signal.SIGTERM)

    def test_terminate_uses_terminate_process(self):
        self.host.terminate()
        self.host.process.terminate.assert_called_once_with()
        self.host.process.send_signal.assert_not_called()

    def test_exit_code_one_is_accepted_only_after_a_request(self):
        self.assertTrue(self.host.is_requested_exit(0))
        self.assertFalse(self.host.is_requested_exit(1))
        self.assertFalse(self.host.is_requested_exit(None))
        self.host.terminate()
        self.assertTrue(self.host.is_requested_exit(1))
        self.assertFalse(self.host.is_requested_exit(2))
        self.assertFalse(self.host.is_requested_exit(-signal.SIGTERM))

    def adopt_browser(self):
        self.host.pid, self.host.identity = BROWSER_PID, (BROWSER_PID, START, self.command_line)

    def test_browser_that_exits_with_launcher_needs_no_extra_signal(self):
        self.adopt_browser()
        with self.rows(), patch.object(runtime.os, "kill") as kill:
            self.host._ensure_windows_browser_gone()
        kill.assert_not_called()

    def test_browser_outliving_launcher_is_terminated_by_exact_identity(self):
        self.adopt_browser()
        alive = [(BROWSER_PID, START, self.command_line)]
        clock = iter(range(0, 1000))

        def terminate(pid, value):
            self.assertEqual((pid, value), (BROWSER_PID, signal.SIGTERM))
            alive.clear()

        with patch.object(runtime.HostProcess, "_process_rows", side_effect=lambda pid=None: list(alive)), \
                patch.object(runtime.time, "monotonic", side_effect=lambda: next(clock)), \
                patch.object(runtime.time, "sleep"), \
                patch.object(runtime.os, "kill", side_effect=terminate) as kill:
            self.host._ensure_windows_browser_gone()
        kill.assert_called_once()

    def test_reused_pid_is_never_signalled(self):
        self.adopt_browser()
        reused = (BROWSER_PID, "2026-11-07T07:00:00.0000000Z", self.command_line)
        with self.rows(reused), patch.object(runtime.os, "kill") as kill:
            self.host._ensure_windows_browser_gone()
        kill.assert_not_called()

    def test_browser_that_cannot_be_stopped_fails_cleanup(self):
        self.adopt_browser()
        clock = iter(range(0, 1000))
        with self.rows((BROWSER_PID, START, self.command_line)), \
                patch.object(runtime.time, "monotonic", side_effect=lambda: next(clock)), \
                patch.object(runtime.time, "sleep"), patch.object(runtime.os, "kill"):
            with self.assertRaisesRegex(RuntimeError, "outlived its launcher"):
                self.host._ensure_windows_browser_gone()


class PosixExitSemanticsTests(unittest.TestCase):
    def test_posix_requires_the_requested_signal_status(self):
        with patch.object(runtime, "IS_WINDOWS", False), patch.object(runtime.subprocess, "Popen") as launch:
            launch.return_value = Mock(pid=LAUNCHER_PID)
            host = runtime.HostProcess(Path("/tmp/floorp"), Path("/tmp/floorp"),
                                       Path("/tmp/profile"), Path("/tmp"), Mock())
            host.requested_signal = signal.SIGTERM
            self.assertTrue(host.is_requested_exit(-signal.SIGTERM))
            self.assertFalse(host.is_requested_exit(1))


if __name__ == "__main__":
    unittest.main()

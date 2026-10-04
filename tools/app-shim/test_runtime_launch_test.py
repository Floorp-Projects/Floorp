#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Non-GUI coverage of smoke-test process ownership and LaunchServices cleanup."""

import importlib.util
from pathlib import Path
import select
import signal
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch


spec = importlib.util.spec_from_file_location("app_shim_runtime_test", Path(__file__).with_name("test-runtime.py"))
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


class HostProcessTests(unittest.TestCase):
    def setUp(self):
        self.browser = Path("/tmp/Exact Test.app")
        self.binary = self.browser / "Contents/MacOS/firefox"
        self.profile = Path("/tmp/private unique/profile")
        self.output = self.profile.parent
        self.process = Mock(pid=987, returncode=None)
        self.process.poll.return_value = None
        self.popen = patch.object(runtime.subprocess, "Popen", return_value=self.process)
        self.launch = self.popen.start()
        self.addCleanup(self.popen.stop)
        self.queue = Mock()
        self.queue.control.return_value = []
        queue_patch = patch.object(runtime.select, "kqueue", return_value=self.queue)
        queue_patch.start()
        self.addCleanup(queue_patch.stop)
        self.host = runtime.HostProcess(self.browser, self.binary, self.profile,
                                        self.output, Mock(), launch_services=True)
        self.row = (123, "Thu Sep 24 01:30:00 2026", " ".join(self.host.command))

    def session(self, pid=123, profile=None):
        return {"capabilities": {"moz:processID": pid,
                "moz:profile": str(profile or self.profile)}}

    def test_exact_app_new_instance_arguments_and_separate_logs(self):
        argv = self.launch.call_args.args[0]
        self.assertEqual(argv[:5], ["/usr/bin/open", "-n", "-W", "-a", str(self.browser)])
        self.assertEqual(argv[argv.index("--args") + 1:], self.host.command[1:])
        self.assertEqual(argv[argv.index("--stdout") + 1], str(self.output / "browser.stdout.log"))
        self.assertEqual(argv[argv.index("--stderr") + 1], str(self.output / "browser.stderr.log"))
        self.assertIsNone(self.host.pid)

    def test_wrong_profile_is_never_adopted(self):
        with self.assertRaisesRegex(RuntimeError, "isolated test profile"):
            self.host.verify_session(self.session(profile=Path("/Users/someone/default-profile")))
        self.assertIsNone(self.host.pid)
        self.assertFalse(self.host.verified)
        self.queue.control.assert_not_called()

    def test_marionette_pid_requires_exact_executable_and_command(self):
        wrong = (123, self.row[1], self.row[2].replace("Exact Test.app", "Other.app"))
        with patch.object(self.host, "_process_rows", return_value=[wrong]):
            with self.assertRaisesRegex(RuntimeError, "exact executable"):
                self.host.verify_session(self.session())
        self.assertFalse(self.host.verified)

    def test_signal_targets_verified_browser_not_open(self):
        with patch.object(self.host, "_process_rows", return_value=[self.row]), patch.object(runtime.os, "kill") as kill:
            self.host.verify_session(self.session())
            self.host.terminate()
        kill.assert_called_once_with(123, signal.SIGTERM)
        self.process.terminate.assert_not_called()

    def test_reused_pid_is_not_signalled(self):
        with patch.object(self.host, "_process_rows", return_value=[self.row]):
            self.host.verify_session(self.session())
        reused = (123, "Thu Sep 24 01:40:00 2026", self.row[2])
        with patch.object(self.host, "_process_rows", return_value=[reused]), patch.object(runtime.os, "kill") as kill:
            self.host.kill()
        kill.assert_not_called()
        self.assertTrue(self.host.exited)

    def test_startup_cleanup_rejects_ambiguous_profile_processes(self):
        other = (456, self.row[1], self.row[2])
        with patch.object(self.host, "_process_rows", return_value=[self.row, other]):
            with self.assertRaisesRegex(RuntimeError, "Multiple processes"):
                self.host._discover_for_cleanup()
        self.assertIsNone(self.host.pid)

    def test_actual_crash_status_is_not_launcher_success(self):
        with patch.object(self.host, "_process_rows", return_value=[self.row]):
            self.host.verify_session(self.session())
        self.process.returncode = 0
        self.queue.control.return_value = [SimpleNamespace(
            ident=123, fflags=select.KQ_NOTE_EXIT | 0x04000000, data=signal.SIGSEGV)]
        self.assertEqual(self.host.returncode, -signal.SIGSEGV)
        self.assertTrue(self.host.exited)


if __name__ == "__main__":
    unittest.main()

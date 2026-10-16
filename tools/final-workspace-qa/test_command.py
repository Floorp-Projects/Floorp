# SPDX-License-Identifier: MPL-2.0
"""Exercise the actual supervisor helper with owned host CLI children only."""

import os
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

import run_linux


@unittest.skipUnless(sys.platform == "linux", "Linux supervisor pipe contract")
class SupervisorCommandTests(unittest.TestCase):
    def setUp(self):
        self.children = []
        self.original_popen = subprocess.Popen

    def spawn(self, *args, **kwargs):
        child = self.original_popen(*args, **kwargs)
        self.children.append(child)
        return child

    def call(self, script, timeout=5):
        with patch("run_linux.subprocess.Popen", side_effect=self.spawn):
            return run_linux.command([sys.executable, "-B", "-c", script], timeout)

    def tearDown(self):
        for child in self.children:
            # Only this test's retained child object is eligible for cleanup.
            if child.returncode is None:
                child.kill()
                child.wait(timeout=5)
                self.fail("Supervisor helper left its own direct child unreaped")

    def test_both_streams_are_drained_and_exact_combined_limit_is_accepted(self):
        half = run_linux.MAX_COMMAND_OUTPUT // 2
        value = self.call(f"import os;os.write(1,b'a'*{half});os.write(2,b'b'*{half})")
        self.assertEqual(value, "a" * half)

    def test_overflow_in_either_stream_kills_and_reaps_the_owned_child(self):
        for descriptor in (1, 2):
            with self.subTest(descriptor=descriptor):
                with self.assertRaisesRegex(ValueError, "diagnostics exceeded"):
                    self.call(f"import os,time;os.write({descriptor},b'x'*({run_linux.MAX_COMMAND_OUTPUT}+1));time.sleep(30)")
                self.assertIsNotNone(self.children[-1].returncode)

    def test_limit_is_combined_not_two_independent_stream_allowances(self):
        half = run_linux.MAX_COMMAND_OUTPUT // 2
        with self.assertRaisesRegex(ValueError, "diagnostics exceeded"):
            self.call(f"import os,time;os.write(1,b'a'*{half});os.write(2,b'b'*({half}+1));time.sleep(30)")

    def test_stream_reads_stop_at_allowance_plus_one_byte(self):
        original_read = os.read
        read_bytes = 0
        def bounded_read(fd, count):
            nonlocal read_bytes
            data = original_read(fd, count)
            read_bytes += len(data)
            return data
        # Popen's exec-error pipe is also read by this wrapper, but is empty for
        # successful exec. The CLI diagnostic streams never allocate the flood.
        with patch("run_linux.os.read", side_effect=bounded_read):
            with self.assertRaisesRegex(ValueError, "diagnostics exceeded"):
                self.call("import os,time\nwhile True: os.write(1,b'x'*65536)")
        self.assertEqual(read_bytes, run_linux.MAX_COMMAND_OUTPUT + 1)

    def test_single_deadline_covers_silent_child_and_eof_before_exit(self):
        for script in ("import time;time.sleep(30)",
                       "import os,time;os.close(1);os.close(2);time.sleep(30)"):
            with self.subTest(script=script):
                started = time.monotonic()
                with self.assertRaises(subprocess.TimeoutExpired):
                    self.call(script, timeout=0.3)
                self.assertLess(time.monotonic() - started, 5)
                self.assertIsNotNone(self.children[-1].returncode)

    def test_stdin_is_null_and_no_open_parent_descriptors_are_inherited(self):
        observed = []
        def spawn(*args, **kwargs):
            observed.append(kwargs)
            return self.spawn(*args, **kwargs)
        with patch("run_linux.subprocess.Popen", side_effect=spawn):
            with tempfile.TemporaryFile() as parent_file:
                os.set_inheritable(parent_file.fileno(), True)
                script = ("import os,sys;print(len(sys.stdin.buffer.read()))\n"
                          f"try: os.fstat({parent_file.fileno()})\n"
                          "except OSError: print('closed')\n"
                          "else: raise RuntimeError('parent descriptor leaked')")
                result = run_linux.command([sys.executable, "-B", "-c", script], 5)
        self.assertEqual(result, "0\nclosed\n")
        self.assertEqual(observed[0]["stdin"], subprocess.DEVNULL)
        self.assertIs(observed[0]["close_fds"], True)
        self.assertEqual(observed[0]["bufsize"], 0)

    def test_invalid_deadline_is_rejected_before_spawning(self):
        with patch("run_linux.subprocess.Popen") as spawn:
            for seconds in (0, -1, True, float("inf"), float("nan"), 31, "30"):
                with self.subTest(seconds=seconds), self.assertRaises(ValueError):
                    run_linux.command([sys.executable], seconds)
            spawn.assert_not_called()

    def test_nonzero_status_and_invalid_utf8_remain_original_failures(self):
        with self.assertRaisesRegex(ValueError, "Original supervisor operation failed: controlled stderr"):
            self.call("import os;os.write(2,b'controlled stderr');raise SystemExit(7)")
        with self.assertRaises(UnicodeDecodeError):
            self.call("import os;os.write(1,b'\\xff')")

    def test_spawn_failure_is_not_replaced(self):
        with self.assertRaises(FileNotFoundError):
            run_linux.command(["/this-controlled-test-command-does-not-exist"], 5)

    def test_successful_wait_that_returns_after_deadline_cannot_pass(self):
        original_clock = time.monotonic
        offset = [0]
        def spawn(*args, **kwargs):
            child = self.spawn(*args, **kwargs)
            original_wait = child.wait
            def wait(*args, **kwargs):
                status = original_wait(*args, **kwargs)
                offset[0] = 31  # Simulate scheduler delay after successful reap.
                return status
            child.wait = wait
            return child
        with patch("run_linux.subprocess.Popen", side_effect=spawn), \
                patch("run_linux.time.monotonic", side_effect=lambda: original_clock() + offset[0]):
            with self.assertRaises(subprocess.TimeoutExpired):
                run_linux.command([sys.executable, "-B", "-c", "print('ready')"], 5)
        self.assertEqual(self.children[-1].returncode, 0)

    def test_annotation_failure_cannot_replace_the_first_error(self):
        class Unnotable(InterruptedError):
            def add_note(self, _note): raise RuntimeError("Controlled note failure")
        first = Unnotable("Controlled first error")
        original_read = os.read
        selected_fds = set()
        class CloseFailure:
            def __init__(self, stream): self.stream = stream
            def fileno(self): return self.stream.fileno()
            def close(self):
                self.stream.close()
                raise OSError("Controlled close failure")
        def spawn(*args, **kwargs):
            child = self.spawn(*args, **kwargs)
            selected_fds.add(child.stdout.fileno())
            child.stdout = CloseFailure(child.stdout)
            return child
        def read(fd, count):
            if fd in selected_fds: raise first
            return original_read(fd, count)
        with patch("run_linux.subprocess.Popen", side_effect=spawn), \
                patch("run_linux.os.read", side_effect=read):
            with self.assertRaises(Unnotable) as caught:
                run_linux.command([sys.executable, "-B", "-c", "print('ready')"], 5)
        self.assertIs(caught.exception, first)

    def test_read_cancel_survives_secondary_close_failure(self):
        first = InterruptedError("Controlled original command cancellation")
        original_read = os.read
        selected_fds = set()
        class CloseFailure:
            def __init__(self, stream): self.stream = stream
            def fileno(self): return self.stream.fileno()
            def close(self):
                self.stream.close()
                raise OSError("Controlled secondary stream close error")
        def spawn(*args, **kwargs):
            child = self.spawn(*args, **kwargs)
            selected_fds.add(child.stdout.fileno())
            child.stdout = CloseFailure(child.stdout)
            return child
        def read(fd, count):
            if fd in selected_fds: raise first
            return original_read(fd, count)
        with patch("run_linux.subprocess.Popen", side_effect=spawn), \
                patch("run_linux.os.read", side_effect=read):
            with self.assertRaises(InterruptedError) as caught:
                run_linux.command([sys.executable, "-B", "-c",
                                   "import os,time;os.write(1,b'ready');time.sleep(30)"], 5)
        self.assertIs(caught.exception, first)
        self.assertIn("Owned supervisor CLI cleanup also failed", first.__notes__)


if __name__ == "__main__":
    unittest.main()

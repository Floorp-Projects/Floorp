#!/usr/bin/env python

import threading
from time import sleep

import mozrunnertest


class RunnerThread(threading.Thread):
    def __init__(self, runner, timeout=10):
        threading.Thread.__init__(self)
        self.runner = runner
        self.timeout = timeout

    def run(self):
        sleep(self.timeout)
        self.runner.stop()


class MozrunnerInteractiveTestCase(mozrunnertest.MozrunnerTestCase):

    def test_run_interactive(self):
        """Bug 965183: Run process in interactive mode and call wait()"""
        pid = self.runner.start(interactive=True)
        self.pids.append(pid)

        thread = RunnerThread(self.runner, 5)
        self.threads.append(thread)
        thread.start()

        # This is a blocking call. So the process should be killed by the thread
        self.runner.wait()
        thread.join()
        self.assertFalse(self.runner.is_running())

    def test_stop_interactive(self):
        """Bug 965183: Explicitely stop process in interactive mode"""
        pid = self.runner.start(interactive=True)
        self.pids.append(pid)

        self.runner.stop()

    def test_wait_after_process_finished(self):
        """Wait after the process has been stopped should not raise an error"""
        self.runner.start(interactive=True)
        sleep(5)
        self.runner.process_handler.kill()

        returncode = self.runner.wait(1)

        self.assertNotIn(returncode, [None, 0])
        self.assertIsNotNone(self.runner.process_handler)

#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import threading
from time import sleep

import mozunit

import mozrunnertest


class RunnerThread(threading.Thread):

    def __init__(self, runner, do_start, timeout=10):
        threading.Thread.__init__(self)
        self.runner = runner
        self.timeout = timeout
        self.do_start = do_start

    def run(self):
        sleep(self.timeout)
        if self.do_start:
            self.runner.start()
        else:
            self.runner.stop()


class MozrunnerThreadsTestCase(mozrunnertest.MozrunnerTestCase):

    def test_process_start_via_thread(self):
        """Start the runner via a thread"""
        thread = RunnerThread(self.runner, True, 2)
        self.threads.append(thread)

        thread.start()
        thread.join()

        self.assertTrue(self.runner.is_running())

    def test_process_stop_via_multiple_threads(self):
        """Stop the runner via multiple threads"""
        self.runner.start()
        for i in range(5):
            thread = RunnerThread(self.runner, False, 5)
            self.threads.append(thread)
            thread.start()

        # Wait until the process has been stopped by another thread
        for thread in self.threads:
            thread.join()
        returncode = self.runner.wait(2)

        self.assertNotIn(returncode, [None, 0])
        self.assertEqual(self.runner.returncode, returncode)
        self.assertIsNotNone(self.runner.process_handler)
        self.assertEqual(self.runner.wait(10), returncode)

    def test_process_post_stop_via_thread(self):
        """Stop the runner and try it again with a thread a bit later"""
        self.runner.start()
        thread = RunnerThread(self.runner, False, 5)
        self.threads.append(thread)
        thread.start()

        # Wait a bit to start the application gets started
        self.runner.wait(2)
        returncode = self.runner.stop()
        thread.join()

        self.assertNotIn(returncode, [None, 0])
        self.assertEqual(self.runner.returncode, returncode)
        self.assertIsNotNone(self.runner.process_handler)
        self.assertEqual(self.runner.wait(10), returncode)


if __name__ == '__main__':
    mozunit.main()

#!/usr/bin/env python

from __future__ import absolute_import

import os

import mozunit

from mozprocess import processhandler

import proctest


here = os.path.dirname(os.path.abspath(__file__))


class ProcTestDetached(proctest.ProcTest):
    """Class to test for detached processes."""

    def test_check_for_detached_before_run(self):
        """Process is not started yet when checked for detached."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here)

        with self.assertRaises(RuntimeError):
            p.check_for_detached(1234)

    def test_check_for_detached_while_running_with_current_pid(self):
        """Process is started, and check for detached with original pid."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here)
        p.run()

        orig_pid = p.pid
        p.check_for_detached(p.pid)

        self.assertEqual(p.pid, orig_pid)
        self.assertIsNone(p.proc.detached_pid)

        self.determine_status(p, True)
        p.kill()

    def test_check_for_detached_after_fork(self):
        """Process is started, and check for detached with new pid."""
        pass

    def test_check_for_detached_after_kill(self):
        """Process is killed before checking for detached pid."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here)
        p.run()
        p.kill()

        orig_pid = p.pid
        p.check_for_detached(p.pid)

        self.assertEqual(p.pid, orig_pid)
        self.assertIsNone(p.proc.detached_pid)

        self.determine_status(p)


if __name__ == '__main__':
    mozunit.main()

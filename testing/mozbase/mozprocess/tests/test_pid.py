#!/usr/bin/env python

from __future__ import absolute_import

import os

import mozunit

from mozprocess import processhandler

import proctest


here = os.path.dirname(os.path.abspath(__file__))


class ProcTestPid(proctest.ProcTest):
    """Class to test process pid."""

    def test_pid_before_run(self):
        """Process is not started, and pid is checked."""
        p = processhandler.ProcessHandler([self.python])
        with self.assertRaises(RuntimeError):
            p.pid

    def test_pid_while_running(self):
        """Process is started, and pid is checked."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here)
        p.run()

        self.assertIsNotNone(p.pid)

        self.determine_status(p, True)
        p.kill()

    def test_pid_after_kill(self):
        """Process is killed, and pid is checked."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here)
        p.run()
        p.kill()

        self.assertIsNotNone(p.pid)
        self.determine_status(p)


if __name__ == '__main__':
    mozunit.main()

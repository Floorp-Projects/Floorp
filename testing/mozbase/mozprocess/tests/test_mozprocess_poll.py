#!/usr/bin/env python

import os
import signal
import unittest

from mozprocess import processhandler

import proctest


here = os.path.dirname(os.path.abspath(__file__))


class ProcTestPoll(proctest.ProcTest):
    """ Class to test process poll """

    def test_poll_before_run(self):
        """Process is not started, and poll() is called"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                          "process_normal_finish_python.ini"],
                                          cwd=here)
        self.assertRaises(RuntimeError, p.poll)

    def test_poll_while_running(self):
        """Process is started, and poll() is called"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                          "process_normal_finish_python.ini"],
                                          cwd=here)
        p.run()
        returncode = p.poll()

        self.assertEqual(returncode, None)

        self.determine_status(p, True)
        p.kill()

    def test_poll_after_kill(self):
        """Process is killed, and poll() is called"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                          "process_normal_finish_python.ini"],
                                          cwd=here)
        p.run()
        returncode = p.kill()

        # We killed the process, so the returncode should be < 0
        self.assertLess(returncode, 0)
        self.assertEqual(returncode, p.poll())

        self.determine_status(p)

    def test_poll_after_kill_no_process_group(self):
        """Process (no group) is killed, and poll() is called"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                          "process_normal_finish_no_process_group.ini"],
                                          cwd=here,
                                          ignore_children=True
                                          )
        p.run()
        returncode = p.kill()

        # We killed the process, so the returncode should be < 0
        self.assertLess(returncode, 0)
        self.assertEqual(returncode, p.poll())

        self.determine_status(p)

    def test_poll_after_double_kill(self):
        """Process is killed twice, and poll() is called"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                          "process_normal_finish_python.ini"],
                                          cwd=here)
        p.run()
        p.kill()
        returncode = p.kill()

        # We killed the process, so the returncode should be < 0
        self.assertLess(returncode, 0)
        self.assertEqual(returncode, p.poll())

        self.determine_status(p)

    def test_poll_after_external_kill(self):
        """Process is killed externally, and poll() is called"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                          "process_normal_finish_python.ini"],
                                          cwd=here)
        p.run()
        os.kill(p.pid, signal.SIGTERM)
        returncode = p.wait()

        # We killed the process, so the returncode should be < 0
        self.assertEqual(returncode, -signal.SIGTERM)
        self.assertEqual(returncode, p.poll())

        self.determine_status(p)


if __name__ == '__main__':
    unittest.main()

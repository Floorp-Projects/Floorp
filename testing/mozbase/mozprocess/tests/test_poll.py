#!/usr/bin/env python

from __future__ import absolute_import

import os
import signal
import sys
import unittest

import mozinfo
import mozunit
import proctest
import time
from mozprocess import processhandler

here = os.path.dirname(os.path.abspath(__file__))


class ProcTestPoll(proctest.ProcTest):
    """Class to test process poll."""

    def test_poll_before_run(self):
        """Process is not started, and poll() is called."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here)
        self.assertRaises(RuntimeError, p.poll)

    def test_poll_while_running(self):
        """Process is started, and poll() is called."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here)
        p.run()
        returncode = p.poll()

        self.assertEqual(returncode, None)

        self.determine_status(p, True)
        p.kill()

    def test_poll_after_kill(self):
        """Process is killed, and poll() is called."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here)
        p.run()
        returncode = p.kill()

        # We killed the process, so the returncode should be non-zero
        if mozinfo.isWin:
            self.assertGreater(returncode, 0,
                               'Positive returncode expected, got "%s"' % returncode)
        else:
            self.assertLess(returncode, 0,
                            'Negative returncode expected, got "%s"' % returncode)

        self.assertEqual(returncode, p.poll())

        self.determine_status(p)

    def test_poll_after_kill_no_process_group(self):
        """Process (no group) is killed, and poll() is called."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish_no_process_group.ini"],
                                          cwd=here,
                                          ignore_children=True
                                          )
        p.run()
        returncode = p.kill()

        # We killed the process, so the returncode should be non-zero
        if mozinfo.isWin:
            self.assertGreater(returncode, 0,
                               'Positive returncode expected, got "%s"' % returncode)
        else:
            self.assertLess(returncode, 0,
                            'Negative returncode expected, got "%s"' % returncode)

        self.assertEqual(returncode, p.poll())

        self.determine_status(p)

    def test_poll_after_double_kill(self):
        """Process is killed twice, and poll() is called."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here)
        p.run()
        p.kill()
        returncode = p.kill()

        # We killed the process, so the returncode should be non-zero
        if mozinfo.isWin:
            self.assertGreater(returncode, 0,
                               'Positive returncode expected, got "%s"' % returncode)
        else:
            self.assertLess(returncode, 0,
                            'Negative returncode expected, got "%s"' % returncode)

        self.assertEqual(returncode, p.poll())

        self.determine_status(p)

    @unittest.skipIf(sys.platform.startswith("win"), "Bug 1493796")
    def test_poll_after_external_kill(self):
        """Process is killed externally, and poll() is called."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here)
        p.run()

        os.kill(p.pid, signal.SIGTERM)

        # Allow the output reader thread to finish processing remaining data
        for i in range(0, 100):
            time.sleep(processhandler.INTERVAL_PROCESS_ALIVE_CHECK)
            returncode = p.poll()
            if returncode is not None:
                break

        # We killed the process, so the returncode should be non-zero
        if mozinfo.isWin:
            self.assertEqual(returncode, signal.SIGTERM,
                             'Positive returncode expected, got "%s"' % returncode)
        else:
            self.assertEqual(returncode, -signal.SIGTERM,
                             '%s expected, got "%s"' % (-signal.SIGTERM, returncode))

        self.assertEqual(returncode, p.wait())

        self.determine_status(p)


if __name__ == '__main__':
    mozunit.main()

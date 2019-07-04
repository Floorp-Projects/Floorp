#!/usr/bin/env python

from __future__ import absolute_import

import os
import signal
import sys

import mozinfo
import mozunit
import proctest
from mozprocess import processhandler

here = os.path.dirname(os.path.abspath(__file__))


class ProcTestWait(proctest.ProcTest):
    """ Class to test process waits and timeouts """

    def test_normal_finish(self):
        """Process is started, runs to completion while we wait for it"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here)
        p.run()
        p.wait()

        self.determine_status(p)

    def test_wait(self):
        """Process is started runs to completion while we wait indefinitely"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_waittimeout_10s.ini"],
                                          cwd=here)
        p.run()
        p.wait()

        self.determine_status(p)

    def test_timeout(self):
        """ Process is started, runs but we time out waiting on it
            to complete
        """
        myenv = None
        # On macosx1014, subprocess fails to find `six` when run with python3.
        # This ensures that subprocess first looks to sys.path to find `six`.
        # See https://bugzilla.mozilla.org/show_bug.cgi?id=1562083
        if sys.platform == 'darwin' and sys.version_info[0] > 2:
            myenv = os.environ.copy()
            myenv['PYTHONPATH'] = ':'.join(sys.path)

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_waittimeout.ini"],
                                          cwd=here, env=myenv)
        p.run(timeout=10)
        p.wait()

        if mozinfo.isUnix:
            # process was killed, so returncode should be negative
            self.assertLess(p.proc.returncode, 0)

        self.determine_status(p, False, ['returncode', 'didtimeout'])

    def test_waittimeout(self):
        """
        Process is started, then wait is called and times out.
        Process is still running and didn't timeout
        """
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_waittimeout_10s.ini"],
                                          cwd=here)

        p.run()
        p.wait(timeout=0)

        self.determine_status(p, True, ())

    def test_waitnotimeout(self):
        """ Process is started, runs to completion before our wait times out
        """
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_waittimeout_10s.ini"],
                                          cwd=here)
        p.run(timeout=30)
        p.wait()

        self.determine_status(p)

    def test_wait_twice_after_kill(self):
        """Bug 968718: Process is started and stopped. wait() twice afterward."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_waittimeout.ini"],
                                          cwd=here)
        p.run()
        p.kill()
        returncode1 = p.wait()
        returncode2 = p.wait()

        self.determine_status(p)

        # We killed the process, so the returncode should be non-zero
        if mozinfo.isWin:
            self.assertGreater(returncode2, 0,
                               'Positive returncode expected, got "%s"' % returncode2)
        else:
            self.assertLess(returncode2, 0,
                            'Negative returncode expected, got "%s"' % returncode2)
        self.assertEqual(returncode1, returncode2,
                         'Expected both returncodes of wait() to be equal')

    def test_wait_after_external_kill(self):
        """Process is killed externally, and poll() is called."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish.ini"],
                                          cwd=here)
        p.run()
        os.kill(p.pid, signal.SIGTERM)
        returncode = p.wait()

        # We killed the process, so the returncode should be non-zero
        if mozinfo.isWin:
            self.assertEqual(returncode, signal.SIGTERM,
                             'Positive returncode expected, got "%s"' % returncode)
        else:
            self.assertEqual(returncode, -signal.SIGTERM,
                             '%s expected, got "%s"' % (-signal.SIGTERM, returncode))

        self.assertEqual(returncode, p.poll())

        self.determine_status(p)


if __name__ == '__main__':
    mozunit.main()

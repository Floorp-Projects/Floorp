#!/usr/bin/env python

import os
import proctest
import mozinfo

import mozunit

from mozprocess import processhandler

here = os.path.dirname(os.path.abspath(__file__))


class ProcTestWait(proctest.ProcTest):
    """ Class to test process waits and timeouts """

    def test_normal_finish(self):
        """Process is started, runs to completion while we wait for it"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish_python.ini"],
                                          cwd=here)
        p.run()
        p.wait()

        self.determine_status(p)

    def test_wait(self):
        """Process is started runs to completion while we wait indefinitely"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_waittimeout_10s_python.ini"],
                                          cwd=here)
        p.run()
        p.wait()

        self.determine_status(p)

    def test_timeout(self):
        """ Process is started, runs but we time out waiting on it
            to complete
        """
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_waittimeout_python.ini"],
                                          cwd=here)
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
                                           "process_waittimeout_10s_python.ini"],
                                          cwd=here)

        p.run()
        p.wait(timeout=5)

        self.determine_status(p, True, ())

    def test_waitnotimeout(self):
        """ Process is started, runs to completion before our wait times out
        """
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_waittimeout_10s_python.ini"],
                                          cwd=here)
        p.run(timeout=30)
        p.wait()

        self.determine_status(p)

    def test_wait_twice_after_kill(self):
        """Bug 968718: Process is started and stopped. wait() twice afterward."""
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_waittimeout_python.ini"],
                                          cwd=here)
        p.run()
        p.kill()
        returncode1 = p.wait()
        returncode2 = p.wait()

        self.determine_status(p)

        self.assertLess(returncode2, 0,
                        'Negative returncode expected, got "%s"' % returncode2)
        self.assertEqual(returncode1, returncode2,
                         'Expected both returncodes of wait() to be equal')


if __name__ == '__main__':
    mozunit.main()

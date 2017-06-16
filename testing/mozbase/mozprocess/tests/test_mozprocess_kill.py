#!/usr/bin/env python

import os
import time
import unittest
import proctest
import signal

import mozunit

from mozprocess import processhandler

here = os.path.dirname(os.path.abspath(__file__))


class ProcTestKill(proctest.ProcTest):
    """ Class to test various process tree killing scenatios """

    def test_kill_before_run(self):
        """Process is not started, and kill() is called"""

        p = processhandler.ProcessHandler([self.python, '-V'])
        self.assertRaises(RuntimeError, p.kill)

    def test_process_kill(self):
        """Process is started, we kill it"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_finish_python.ini"],
                                          cwd=here)
        p.run()
        p.kill()

        self.determine_status(p, expectedfail=('returncode',))

    def test_process_kill_deep(self):
        """Process is started, we kill it, we use a deep process tree"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_deep_python.ini"],
                                          cwd=here)
        p.run()
        p.kill()

        self.determine_status(p, expectedfail=('returncode',))

    def test_process_kill_deep_wait(self):
        """Process is started, we use a deep process tree, we let it spawn
           for a bit, we kill it"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_deep_python.ini"],
                                          cwd=here)
        p.run()
        # Let the tree spawn a bit, before attempting to kill
        time.sleep(3)
        p.kill()

        self.determine_status(p, expectedfail=('returncode',))

    def test_process_kill_broad(self):
        """Process is started, we kill it, we use a broad process tree"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_broad_python.ini"],
                                          cwd=here)
        p.run()
        p.kill()

        self.determine_status(p, expectedfail=('returncode',))

    @unittest.skipUnless(processhandler.isPosix, "posix only")
    def test_process_kill_with_sigterm(self):
        script = os.path.join(here, 'infinite_loop.py')
        p = processhandler.ProcessHandler([self.python, script])

        p.run()
        p.kill()

        self.assertEquals(p.proc.returncode, -signal.SIGTERM)

    @unittest.skipUnless(processhandler.isPosix, "posix only")
    def test_process_kill_with_sigint_if_needed(self):
        script = os.path.join(here, 'infinite_loop.py')
        p = processhandler.ProcessHandler([self.python, script, 'deadlock'])

        p.run()
        time.sleep(1)
        p.kill()

        self.assertEquals(p.proc.returncode, -signal.SIGKILL)


if __name__ == '__main__':
    mozunit.main()

#!/usr/bin/env python

from __future__ import absolute_import

import os
import time
import unittest
import proctest
import signal
import sys

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
                                           "process_normal_finish.ini"],
                                          cwd=here)
        p.run()
        p.kill()

        self.determine_status(p, expectedfail=('returncode',))

    def test_process_kill_deep(self):
        """Process is started, we kill it, we use a deep process tree"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_deep.ini"],
                                          cwd=here)
        p.run()
        p.kill()

        self.determine_status(p, expectedfail=('returncode',))

    def test_process_kill_deep_wait(self):
        """Process is started, we use a deep process tree, we let it spawn
           for a bit, we kill it"""

        myenv = None
        # On macosx1014, subprocess fails to find `six` when run with python3.
        # This ensures that subprocess first looks to sys.path to find `six`.
        # See https://bugzilla.mozilla.org/show_bug.cgi?id=1562083
        if sys.platform == 'darwin' and sys.version_info[0] > 2:
            myenv = os.environ.copy()
            myenv['PYTHONPATH'] = ':'.join(sys.path)

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_deep.ini"],
                                          cwd=here, env=myenv)
        p.run()
        # Let the tree spawn a bit, before attempting to kill
        time.sleep(3)
        p.kill()

        self.determine_status(p, expectedfail=('returncode',))

    def test_process_kill_broad(self):
        """Process is started, we kill it, we use a broad process tree"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_broad.ini"],
                                          cwd=here)
        p.run()
        p.kill()

        self.determine_status(p, expectedfail=('returncode',))

    def test_process_kill_broad_delayed(self):
        """Process is started, we use a broad process tree, we let it spawn
           for a bit, we kill it"""

        myenv = None
        # On macosx1014, subprocess fails to find `six` when run with python3.
        # This ensures that subprocess first looks to sys.path to find `six`.
        # See https://bugzilla.mozilla.org/show_bug.cgi?id=1562083
        if sys.platform == 'darwin' and sys.version_info[0] > 2:
            myenv = os.environ.copy()
            myenv['PYTHONPATH'] = ':'.join(sys.path)

        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                           "process_normal_broad.ini"],
                                          cwd=here, env=myenv)
        p.run()
        # Let the tree spawn a bit, before attempting to kill
        time.sleep(3)
        p.kill()

        self.determine_status(p, expectedfail=('returncode',))

    @unittest.skipUnless(processhandler.isPosix, "posix only")
    def test_process_kill_with_sigterm(self):
        script = os.path.join(here, 'scripts', 'infinite_loop.py')
        p = processhandler.ProcessHandler([self.python, script])

        p.run()
        p.kill()

        self.assertEquals(p.proc.returncode, -signal.SIGTERM)

    @unittest.skipUnless(processhandler.isPosix, "posix only")
    def test_process_kill_with_sigint_if_needed(self):
        script = os.path.join(here, 'scripts', 'infinite_loop.py')
        p = processhandler.ProcessHandler([self.python, script, 'deadlock'])

        p.run()
        time.sleep(1)
        p.kill()

        self.assertEquals(p.proc.returncode, -signal.SIGKILL)


if __name__ == '__main__':
    mozunit.main()

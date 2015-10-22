#!/usr/bin/env python

import os
import time
import unittest
import proctest
from mozprocess import processhandler

here = os.path.dirname(os.path.abspath(__file__))

class ProcTestKill(proctest.ProcTest):
    """ Class to test various process tree killing scenatios """

    # This test should ideally be a part of test_mozprocess_kill.py
    # It has been separated for the purpose of tempporarily disabling it.
    # See https://bugzilla.mozilla.org/show_bug.cgi?id=921632
    def test_process_kill_broad_wait(self):
        """Process is started, we use a broad process tree, we let it spawn
           for a bit, we kill it"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch, "process_normal_broad_python.ini"],
                                          cwd=here)
        p.run()
        # Let the tree spawn a bit, before attempting to kill
        time.sleep(3)
        p.kill()

        self.determine_status(p, expectedfail=('returncode',))

if __name__ == '__main__':
    unittest.main()

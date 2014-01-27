#!/usr/bin/env python

import os
import time
import unittest
import proctest
from mozprocess import processhandler

here = os.path.dirname(os.path.abspath(__file__))

class ProcTestKill(proctest.ProcTest):
    """ Class to test various process tree killing scenatios """

    def test_process_kill(self):
        """Process is started, we kill it"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch, "process_normal_finish_python.ini"],
                                          cwd=here)
        p.run()
        p.kill()

        detected, output = proctest.check_for_process(self.proclaunch)
        self.determine_status(detected,
                              output,
                              p.proc.returncode,
                              p.didTimeout,
                              expectedfail=('returncode',))

    def test_process_kill_deep(self):
        """Process is started, we kill it, we use a deep process tree"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch, "process_normal_deep_python.ini"],
                                          cwd=here)
        p.run()
        p.kill()

        detected, output = proctest.check_for_process(self.proclaunch)
        self.determine_status(detected,
                              output,
                              p.proc.returncode,
                              p.didTimeout,
                              expectedfail=('returncode',))

    def test_process_kill_deep_wait(self):
        """Process is started, we use a deep process tree, we let it spawn
           for a bit, we kill it"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch, "process_normal_deep_python.ini"],
                                          cwd=here)
        p.run()
        # Let the tree spawn a bit, before attempting to kill
        time.sleep(3)
        p.kill()

        detected, output = proctest.check_for_process(self.proclaunch)
        self.determine_status(detected,
                              output,
                              p.proc.returncode,
                              p.didTimeout,
                              expectedfail=('returncode',))

    def test_process_kill_broad(self):
        """Process is started, we kill it, we use a broad process tree"""

        p = processhandler.ProcessHandler([self.python, self.proclaunch, "process_normal_broad_python.ini"],
                                          cwd=here)
        p.run()
        p.kill()

        detected, output = proctest.check_for_process(self.proclaunch)
        self.determine_status(detected,
                              output,
                              p.proc.returncode,
                              p.didTimeout,
                              expectedfail=('returncode',))

if __name__ == '__main__':
    unittest.main()

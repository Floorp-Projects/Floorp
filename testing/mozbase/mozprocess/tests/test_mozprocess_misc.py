#!/usr/bin/env python

import os
import time
import unittest
import proctest
from mozprocess import processhandler

here = os.path.dirname(os.path.abspath(__file__))

class ProcTestMisc(proctest.ProcTest):
    """ Class to test misc operations """

    def test_process_output_twice(self):
        """
        Process is started, then processOutput is called a second time explicitly
        """
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                          "process_waittimeout_10s_python.ini"],
                                          cwd=here)

        p.run()
        p.processOutput(timeout=5)
        p.wait()

        detected, output = proctest.check_for_process(self.proclaunch)
        self.determine_status(detected,
                              output,
                              p.proc.returncode,
                              p.didTimeout,
                              False,
                              ())

if __name__ == '__main__':
    unittest.main()

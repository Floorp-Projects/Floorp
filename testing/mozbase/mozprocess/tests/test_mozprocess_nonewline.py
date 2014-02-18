#!/usr/bin/env python

import os
import unittest
import proctest
from mozprocess import processhandler

here = os.path.dirname(os.path.abspath(__file__))

class ProcTestMisc(proctest.ProcTest):
    """ Class to test misc operations """

    def test_process_output_nonewline(self):
        """
        Process is started, outputs data with no newline
        """
        p = processhandler.ProcessHandler([self.python, "procnonewline.py"],
                                          cwd=here)

        p.run()
        p.processOutput(timeout=5)
        p.wait()

        detected, output = proctest.check_for_process("procnonewline.py")
        self.determine_status(detected,
                              output,
                              p.proc.returncode,
                              p.didTimeout,
                              False,
                              ())

if __name__ == '__main__':
    unittest.main()

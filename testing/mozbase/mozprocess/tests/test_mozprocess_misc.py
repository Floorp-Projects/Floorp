#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
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

        self.determine_status(p, False, ())

    def test_unicode_in_environment(self):
        env = {
            'FOOBAR': 'ʘ',
        }
        p = processhandler.ProcessHandler([self.python, self.proclaunch,
                                          "process_normal_finish_python.ini"],
                                          cwd=here, env=env)
        # passes if no exceptions are raised
        p.run()
        p.wait()

if __name__ == '__main__':
    unittest.main()

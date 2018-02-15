from __future__ import absolute_import

import os
import sys
import unittest

from mozprocess import ProcessHandler


here = os.path.dirname(os.path.abspath(__file__))


class ProcTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.proclaunch = os.path.join(here, "proclaunch.py")
        cls.python = sys.executable

    def determine_status(self, proc, isalive=False, expectedfail=()):
        """
        Use to determine if the situation has failed.
        Parameters:
            proc -- the processhandler instance
            isalive -- Use True to indicate we pass if the process exists; however, by default
                       the test will pass if the process does not exist (isalive == False)
            expectedfail -- Defaults to [], used to indicate a list of fields
                            that are expected to fail
        """
        returncode = proc.proc.returncode
        didtimeout = proc.didTimeout
        detected = ProcessHandler.pid_exists(proc.pid)
        output = ''
        # ProcessHandler has output when store_output is set to True in the constructor
        # (this is the default)
        if getattr(proc, 'output'):
            output = proc.output

        if 'returncode' in expectedfail:
            self.assertTrue(returncode, "Detected an unexpected return code of: %s" % returncode)
        elif isalive:
            self.assertEqual(returncode, None, "Detected not None return code of: %s" % returncode)
        else:
            self.assertNotEqual(returncode, None, "Detected unexpected None return code of")

        if 'didtimeout' in expectedfail:
            self.assertTrue(didtimeout, "Detected that process didn't time out")
        else:
            self.assertTrue(not didtimeout, "Detected that process timed out")

        if isalive:
            self.assertTrue(detected, "Detected process is not running, "
                            "process output: %s" % output)
        else:
            self.assertTrue(not detected, "Detected process is still running, "
                            "process output: %s" % output)

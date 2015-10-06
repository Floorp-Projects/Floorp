import os
import sys
import unittest
import psutil

here = os.path.dirname(os.path.abspath(__file__))


def check_for_process(processName):
    """
        Use to determine if process of the given name is still running.

        Returns:
        detected -- True if process is detected to exist, False otherwise
        output -- if process exists, stdout of the process, [] otherwise
    """
    name = os.path.basename(processName)
    process = [p.pid for p in psutil.process_iter()
               if p.name() == name]

    if process:
        return True, process
    return False, []


class ProcTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.proclaunch = os.path.join(here, "proclaunch.py")
        cls.python = sys.executable

    def determine_status(self,
                         detected=False,
                         output='',
                         returncode=0,
                         didtimeout=False,
                         isalive=False,
                         expectedfail=()):
        """
        Use to determine if the situation has failed.
        Parameters:
            detected -- value from check_for_process to determine if the process is detected
            output -- string of data from detected process, can be ''
            returncode -- return code from process, defaults to 0
            didtimeout -- True if process timed out, defaults to False
            isalive -- Use True to indicate we pass if the process exists; however, by default
                       the test will pass if the process does not exist (isalive == False)
            expectedfail -- Defaults to [], used to indicate a list of fields that are expected to fail
        """
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
            self.assertTrue(detected, "Detected process is not running, process output: %s" % output)
        else:
            self.assertTrue(not detected, "Detected process is still running, process output: %s" % output)

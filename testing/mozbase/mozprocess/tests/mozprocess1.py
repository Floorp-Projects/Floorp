#!/usr/bin/env python

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version 
# 1.1 (the "License"); you may not use this file except in compliance with 
# the License. You may obtain a copy of the License at 
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Clint Talbert <ctalbert@mozilla.com>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

import os
import subprocess
import sys
import unittest
from time import sleep

from mozprocess import processhandler

here = os.path.dirname(os.path.abspath(__file__))

def make_proclaunch(aDir):
    """
        Makes the proclaunch executable.
        Params:
            aDir - the directory in which to issue the make commands
        Returns:
            the path to the proclaunch executable that is generated
    """
    p = subprocess.call(["make"], cwd=aDir)
    if sys.platform == "win32":
        exepath = os.path.join(aDir, "proclaunch.exe")
    else:
        exepath = os.path.join(aDir, "proclaunch")
    return exepath

def check_for_process(processName):
    """
        Use to determine if process of the given name is still running.

        Returns:
        detected -- True if process is detected to exist, False otherwise
        output -- if process exists, stdout of the process, '' otherwise
    """
    output = ''
    if sys.platform == "win32":
        # On windows we use tasklist
        p1 = subprocess.Popen(["tasklist"], stdout=subprocess.PIPE)
        output = p1.communicate()[0]
        detected = False
        for line in output:
            if processName in line:
                detected = True
                break
    else:
        p1 = subprocess.Popen(["ps", "-A"], stdout=subprocess.PIPE)
        p2 = subprocess.Popen(["grep", processName], stdin=p1.stdout, stdout=subprocess.PIPE)
        p1.stdout.close()
        output = p2.communicate()[0]
        detected = False
        for line in output:
            if "grep %s" % processName in line:
                continue
            elif processName in line:
                detected = True
                break

    return detected, output


class ProcTest1(unittest.TestCase):

    def __init__(self, *args, **kwargs):

        # Ideally, I'd use setUpClass but that only exists in 2.7.
        # So, we'll do this make step now.
        self.proclaunch = make_proclaunch(here)
        unittest.TestCase.__init__(self, *args, **kwargs)

    def test_process_normal_finish(self):
        """Process is started, runs to completion while we wait for it"""

        p = processhandler.ProcessHandler([self.proclaunch, "process_normal_finish.ini"],
                                          cwd=here)
        p.run()
        p.waitForFinish()

        detected, output = check_for_process(self.proclaunch)
        self.determine_status(detected,
                              output,
                              p.proc.returncode,
                              p.didTimeout)

    def test_process_waittimeout(self):
        """ Process is started, runs but we time out waiting on it
            to complete
        """
        p = processhandler.ProcessHandler([self.proclaunch, "process_waittimeout.ini"],
                                          cwd=here)
        p.run()
        p.waitForFinish(timeout=10)

        detected, output = check_for_process(self.proclaunch)
        self.determine_status(detected,
                              output,
                              p.proc.returncode,
                              p.didTimeout,
                              False,
                              ['returncode', 'didtimeout'])

    def test_process_kill(self):
        """ Process is started, we kill it
        """
        p = processhandler.ProcessHandler([self.proclaunch, "process_normal_finish.ini"],
                                          cwd=here)
        p.run()
        p.kill()

        detected, output = check_for_process(self.proclaunch)
        self.determine_status(detected,
                              output,
                              p.proc.returncode,
                              p.didTimeout)

    def determine_status(self,
                         detected=False,
                         output='',
                         returncode=0,
                         didtimeout=False,
                         isalive=False,
                         expectedfail=[]):
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
            self.assertTrue(returncode, "Detected an expected non-zero return code")
        else:
            self.assertTrue(returncode == 0, "Detected non-zero return code of: %d" % returncode)

        if 'didtimeout' in expectedfail:
            self.assertTrue(didtimeout, "Process timed out as expected")
        else:
            self.assertTrue(not didtimeout, "Detected that process timed out")

        if detected:
            self.assertTrue(isalive, "Detected process is still running, process output: %s" % output)
        else:
            self.assertTrue(not isalive, "Process ended")

if __name__ == '__main__':
    unittest.main()

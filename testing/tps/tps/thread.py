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
# The Original Code is TPS.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jonathan Griffin <jgriffin@mozilla.com>
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

from threading import Thread

from testrunner import TPSTestRunner

class TPSTestThread(Thread):

  def __init__(self, extensionDir, builddata=None, emailresults=False,
               testfile=None, logfile=None, rlock=None, config=None,
               autolog=False):
    assert(builddata)
    assert(config)
    self.extensionDir = extensionDir
    self.builddata = builddata
    self.emailresults = emailresults
    self.testfile = testfile
    self.logfile = logfile
    self.rlock = rlock
    self.config = config
    self.autolog = autolog
    Thread.__init__(self)

  def run(self):
    # run the tests in normal mode ...
    TPS = TPSTestRunner(self.extensionDir,
                        emailresults=self.emailresults,
                        testfile=self.testfile,
                        logfile=self.logfile,
                        binary=self.builddata['buildurl'],
                        config=self.config,
                        rlock=self.rlock,
                        mobile=False,
                        autolog=self.autolog)
    TPS.run_tests()

    # Get the binary used by this TPS instance, and use it in subsequent
    # ones, so it doesn't have to be re-downloaded each time.
    binary = TPS.firefoxRunner.binary

    # ... and then again in mobile mode
    TPS_mobile = TPSTestRunner(self.extensionDir,
                               emailresults=self.emailresults,
                               testfile=self.testfile,
                               logfile=self.logfile,
                               binary=binary,
                               config=self.config,
                               rlock=self.rlock,
                               mobile=True,
                               autolog=self.autolog)
    TPS_mobile.run_tests()

    # ... and again via the staging server, if credentials are present
    stageaccount = self.config.get('stageaccount')
    if stageaccount:
      username = stageaccount.get('username')
      password = stageaccount.get('password')
      passphrase = stageaccount.get('passphrase')
      if username and password and passphrase:
        stageconfig = self.config.copy()
        stageconfig['account'] = stageaccount.copy()
        TPS_stage = TPSTestRunner(self.extensionDir,
                                  emailresults=self.emailresults,
                                  testfile=self.testfile,
                                  logfile=self.logfile,
                                  binary=binary,
                                  config=stageconfig,
                                  rlock=self.rlock,
                                  mobile=False,
                                  autolog=self.autolog)
        TPS_stage.run_tests()

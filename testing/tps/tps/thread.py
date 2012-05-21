# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

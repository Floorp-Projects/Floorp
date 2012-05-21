# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import logging
import socket

from pulsebuildmonitor import PulseBuildMonitor

from tps.thread import TPSTestThread

class TPSPulseMonitor(PulseBuildMonitor):
  """Listens to pulse messages, and initiates a TPS test run when
     a relevant 'build complete' message is received.
  """

  def __init__(self, extensionDir, platform='linux', config=None,
               autolog=False, emailresults=False, testfile=None,
               logfile=None, rlock=None, **kwargs):
    self.buildtype = ['opt']
    self.autolog = autolog
    self.emailresults = emailresults
    self.testfile = testfile
    self.logfile = logfile
    self.rlock = rlock
    self.extensionDir = extensionDir
    self.config = config
    self.tree = self.config.get('tree', ['services-central'])
    self.platform = [self.config.get('platform', 'linux')]
    self.label=('crossweave@mozilla.com|tps_build_monitor_' +
                socket.gethostname())

    self.logger = logging.getLogger('tps_pulse')
    self.logger.setLevel(logging.DEBUG)
    handler = logging.FileHandler('tps_pulse.log')
    self.logger.addHandler(handler)

    PulseBuildMonitor.__init__(self,
                               trees=self.tree,
                               label=self.label,
                               logger=self.logger,
                               platforms=self.platform,
                               buildtypes=self.buildtype,
                               builds=True,
                               **kwargs)

  def onPulseMessage(self, data):
    key = data['_meta']['routing_key']
    #print key

  def onBuildComplete(self, builddata):
    print "================================================================="
    print json.dumps(builddata)
    print "================================================================="

    thread = TPSTestThread(self.extensionDir,
                           builddata=builddata,
                           emailresults=self.emailresults,
                           autolog=self.autolog,
                           testfile=self.testfile,
                           logfile=self.logfile,
                           rlock=self.rlock,
                           config=self.config)
    thread.daemon = True
    thread.start()

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
    self.buildtype = 'opt'
    self.autolog = autolog
    self.emailresults = emailresults
    self.testfile = testfile
    self.logfile = logfile
    self.rlock = rlock
    self.extensionDir = extensionDir
    self.config = config
    self.tree = self.config.get('tree', ['services-central', 'places'])
    self.platform = self.config.get('platform', 'linux')
    self.label=('crossweave@mozilla.com|tps_build_monitor_' +
                socket.gethostname())

    self.logger = logging.getLogger('tps_pulse')
    self.logger.setLevel(logging.DEBUG)
    handler = logging.FileHandler('tps_pulse.log')
    self.logger.addHandler(handler)

    PulseBuildMonitor.__init__(self,
                               tree=self.tree,
                               label=self.label,
                               mobile=False,
                               logger=self.logger,
                               **kwargs)

  def onPulseMessage(self, data):
    key = data['_meta']['routing_key']
    #print key

  def onBuildComplete(self, builddata):
    print "================================================================="
    print json.dumps(builddata)
    print "================================================================="
    try:
      if not (builddata['platform'] == self.platform and
              builddata['buildtype'] == self.buildtype):
        return
    except KeyError:
      return
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

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
import optparse
import os
import sys
import time
import traceback

from threading import RLock

from tps import TPSFirefoxRunner, TPSPulseMonitor, TPSTestRunner

def main():
  parser = optparse.OptionParser()
  parser.add_option("--email-results",
                    action = "store_true", dest = "emailresults",
                    default = False,
                    help = "email the test results to the recipients defined "
                           "in the config file")
  parser.add_option("--mobile",
                    action = "store_true", dest = "mobile",
                    default = False,
                    help = "run with mobile settings")
  parser.add_option("--autolog",
                    action = "store_true", dest = "autolog",
                    default = False,
                    help = "post results to Autolog")
  parser.add_option("--testfile",
                    action = "store", type = "string", dest = "testfile",
                    default = '../../services/sync/tests/tps/test_sync.js',
                    help = "path to the test file to run "
                           "[default: %default]")
  parser.add_option("--logfile",
                    action = "store", type = "string", dest = "logfile",
                    default = 'tps.log',
                    help = "path to the log file [default: %default]")
  parser.add_option("--binary",
                    action = "store", type = "string", dest = "binary",
                    default = None,
                    help = "path to the Firefox binary, specified either as "
                           "a local file or a url; if omitted, the PATH "
                           "will be searched;")
  parser.add_option("--configfile",
                    action = "store", type = "string", dest = "configfile",
                    default = None,
                    help = "path to the config file to use "
                           "[default: %default]")
  parser.add_option("--pulsefile",
                    action = "store", type = "string", dest = "pulsefile",
                    default = None,
                    help = "path to file containing a pulse message in "
                           "json format that you want to inject into the monitor")
  (options, args) = parser.parse_args()

  configfile = options.configfile
  if configfile is None:
    if os.environ.get('VIRTUAL_ENV'):
      configfile = os.path.join(os.path.dirname(__file__), 'config.json')
    if configfile is None or not os.access(configfile, os.F_OK):
      raise Exception("Unable to find config.json in a VIRTUAL_ENV; you must "
                      "specify a config file using the --configfile option")

  # load the config file
  f = open(configfile, 'r')
  configcontent = f.read()
  f.close()
  config = json.loads(configcontent)

  rlock = RLock()
 
  extensionDir = config.get("extensiondir")
  if not extensionDir or extensionDir == '__EXTENSIONDIR__':
    extensionDir = os.path.join(os.getcwd(), "..", "..", "services", "sync", "tps")
  else:
    if sys.platform == 'win32':
      # replace msys-style paths with proper Windows paths
      import re
      m = re.match('^\/\w\/', extensionDir)
      if m:
        extensionDir = "%s:/%s" % (m.group(0)[1:2], extensionDir[3:])
        extensionDir = extensionDir.replace("/", "\\")

  if options.binary is None:
    while True:
      try:
        # If no binary is specified, start the pulse build monitor, and wait
        # until we receive build notifications before running tests.
        monitor = TPSPulseMonitor(extensionDir,
                                  config=config,
                                  autolog=options.autolog,
                                  emailresults=options.emailresults,
                                  testfile=options.testfile,
                                  logfile=options.logfile,
                                  rlock=rlock)
        print "waiting for pulse build notifications"

        if options.pulsefile:
          # For testing purposes, inject a pulse message directly into
          # the monitor.
          builddata = json.loads(open(options.pulsefile, 'r').read())
          monitor.onBuildComplete(builddata)

        monitor.listen()
      except KeyboardInterrupt:
        sys.exit()
      except:
        traceback.print_exc()
        print 'sleeping 5 minutes'
        time.sleep(300)

  TPS = TPSTestRunner(extensionDir,
                      emailresults=options.emailresults,
                      testfile=options.testfile,
                      logfile=options.logfile,
                      binary=options.binary,
                      config=config,
                      rlock=rlock,
                      mobile=options.mobile,
                      autolog=options.autolog)
  TPS.run_tests()

if __name__ == "__main__":
  main()

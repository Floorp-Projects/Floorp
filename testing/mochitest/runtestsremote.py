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
# The Initial Developer of the Original Code is Joel Maher.
#
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
# Joel Maher <joel.maher@gmail.com> (Original Developer)
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

import sys
import os
import time

sys.path.insert(0, os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0]))))

from automation import Automation
from runtests import Mochitest
from runtests import MochitestOptions

import devicemanager

class RemoteAutomation(Automation):
    _devicemanager = None
    
    def __init__(self, deviceManager, product):
        self._devicemanager = deviceManager
        self._product = product
        Automation.__init__(self)

    def setDeviceManager(self, deviceManager):
        self._devicemanager = deviceManager
        
    def setProduct(self, productName):
        self._product = productName
        
    def waitForFinish(self, proc, utilityPath, timeout, maxTime, startTime):
        status = proc.wait()
        print proc.stdout
        # todo: consider pulling log file from remote
        return status
        
    def buildCommandLine(self, app, debuggerInfo, profileDir, testURL, extraArgs):
        cmd, args = Automation.buildCommandLine(self, app, debuggerInfo, profileDir, testURL, extraArgs)
        # Remove -foreground if it exists, if it doesn't this just returns
        try:
          args.remove('-foreground')
        except:
          pass
        return app, ['--environ:NO_EM_RESTART=1'] + args

    def Process(self, cmd, stdout = None, stderr = None, env = None, cwd = '.'):
        return self.RProcess(self._devicemanager, self._product, cmd, stdout, stderr, env, cwd)

    # be careful here as this inner class doesn't have access to outer class members    
    class RProcess(object):
        # device manager process
        dm = None
        def __init__(self, dm, product, cmd, stdout = None, stderr = None, env = None, cwd = '.'):
            self.dm = dm
            print "going to launch process: " + str(self.dm.host)
            self.proc = dm.launchProcess(cmd)
            exepath = cmd[0]
            name = exepath.split('/')[-1]
            self.procName = name

            # Setting timeout at 1 hour since on a remote device this takes much longer
            self.timeout = 3600
            time.sleep(5)

        @property
        def pid(self):
            hexpid = self.dm.processExist(self.procName)
            if (hexpid == '' or hexpid == None):
                hexpid = 0
            return int(hexpid, 0)
    
        @property
        def stdout(self):
            return self.dm.getFile(self.proc)
 
        def wait(self, timeout = None):
            timer = 0

            if timeout == None:
                timeout = self.timeout

            while (self.dm.process.isAlive()):
                time.sleep(1)
                timer += 1
                if (timer > timeout):
                    break
        
            if (timer >= timeout):
                return 1
            return 0
 
        def kill(self):
            self.dm.killProcess(self.procName)
 

class RemoteOptions(MochitestOptions):

    def __init__(self, automation, scriptdir, **kwargs):
        defaults = {}
        MochitestOptions.__init__(self, automation, scriptdir)

        self.add_option("--deviceIP", action="store",
                    type = "string", dest = "deviceIP",
                    help = "ip address of remote device to test")
        defaults["deviceIP"] = None

        self.add_option("--devicePort", action="store",
                    type = "string", dest = "devicePort",
                    help = "port of remote device to test")
        defaults["devicePort"] = 27020

        self.add_option("--remoteProductName", action="store",
                    type = "string", dest = "remoteProductName",
                    help = "The executable's name of remote product to test - either fennec or firefox, defaults to fennec.exe")
        defaults["remoteProductName"] = "fennec.exe"

        self.add_option("--remote-logfile", action="store",
                    type = "string", dest = "remoteLogFile",
                    help = "Name of log file on the device.  PLEASE ENSURE YOU HAVE CORRECT \ or / FOR THE PATH.")
        defaults["remoteLogFile"] = None

        self.add_option("--remote-webserver", action = "store",
                    type = "string", dest = "remoteWebServer",
                    help = "ip address where the remote web server is hosted at")
        defaults["remoteWebServer"] = None

        self.add_option("--http-port", action = "store",
                    type = "string", dest = "httpPort",
                    help = "ip address where the remote web server is hosted at")
        defaults["httpPort"] = automation.DEFAULT_HTTP_PORT

        self.add_option("--ssl-port", action = "store",
                    type = "string", dest = "sslPort",
                    help = "ip address where the remote web server is hosted at")
        defaults["sslPort"] = automation.DEFAULT_SSL_PORT

        defaults["logFile"] = "mochitest.log"
        if (automation._product == "fennec"):
          defaults["xrePath"] = "/tests/" + automation._product + "/xulrunner"
        else:
          defaults["xrePath"] = "/tests/" + automation._product
        defaults["utilityPath"] = "/tests/bin"
        defaults["certPath"] = "/tests/certs"
        defaults["autorun"] = True
        defaults["closeWhenDone"] = True
        defaults["testPath"] = ""
        defaults["app"] = "/tests/" + automation._product + "/" + defaults["remoteProductName"]

        self.set_defaults(**defaults)

    def verifyOptions(self, options, mochitest):
        # since we are reusing verifyOptions, it will exit if App is not found
        temp = options.app
        options.app = sys.argv[0]
        tempPort = options.httpPort
        tempSSL = options.sslPort
        options = MochitestOptions.verifyOptions(self, options, mochitest)
        options.app = temp
        options.sslPort = tempSSL
        options.httpPort = tempPort

        if (options.remoteWebServer == None):
          print "ERROR: you must provide a remote webserver ip address"
          return None
        else:
          options.webServer = options.remoteWebServer

        if (options.deviceIP == None):
          print "ERROR: you must provide a device IP"
          return None

        if (options.remoteLogFile == None):
          print "ERROR: you must specify a remote log file and ensure you have the correct \ or / slashes"
          return None

        # Set up our options that we depend on based on the above
        options.utilityPath = "/tests/" + mochitest._automation._product + "/bin"
        options.app = "/tests/" + mochitest._automation._product + "/" + options.remoteProductName
        if (mochitest._automation._product == "fennec"):
            options.xrePath = "/tests/" + mochitest._automation._product + "/xulrunner"
        else:
            options.xrePath = options.utilityPath

        return options 

class MochiRemote(Mochitest):

    _automation = None
    _dm = None

    def __init__(self, automation, devmgr, options):
        self._automation = automation
        Mochitest.__init__(self, self._automation)
        self._dm = devmgr
        self.runSSLTunnel = False
        self.remoteProfile = "/tests/profile"
        self.remoteLog = options.remoteLogFile

    def cleanup(self, manifest, options):
        self._dm.getFile(self.remoteLog, self.localLog)
        self._dm.removeFile(self.remoteLog)
        self._dm.removeDir(self.remoteProfile)

    def startWebServer(self, options):
        pass
        
    def stopWebServer(self, options):
        pass
        
    def runExtensionRegistration(self, options, browserEnv):
        pass
        
    def buildProfile(self, options):
        manifest = Mochitest.buildProfile(self, options)
        self.localProfile = options.profilePath
        if self._dm.pushDir(options.profilePath, self.remoteProfile) == None:
            raise devicemanager.FileError("Unable to copy profile to device.")

        options.profilePath = self.remoteProfile
        return manifest
        
    def buildURLOptions(self, options):
        self.localLog = options.logFile
        options.logFile = self.remoteLog
        retVal = Mochitest.buildURLOptions(self, options)
        options.logFile = self.localLog
        return retVal

    def installChromeFile(self, filename, options):
        path = '/'.join(options.app.split('/')[:-1])
        manifest = path + "/chrome/" + os.path.basename(filename)
        if self._dm.pushFile(filename, manifest) == None:
            raise devicemanager.FileError("Unable to install Chrome files on device.")
        return manifest

    def getLogFilePath(self, logFile):             
        return logFile

def main():
    scriptdir = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))
    dm = devicemanager.DeviceManager(None, None)
    auto = RemoteAutomation(dm, "fennec")
    parser = RemoteOptions(auto, scriptdir)
    options, args = parser.parse_args()

    dm = devicemanager.DeviceManager(options.deviceIP, options.devicePort)
    auto.setDeviceManager(dm)

    productPieces = options.remoteProductName.split('.')
    if (productPieces != None):
      auto.setProduct(productPieces[0])
    else:
      auto.setProduct(options.remoteProductName)

    mochitest = MochiRemote(auto, dm, options)

    options = parser.verifyOptions(options, mochitest)
    if (options == None):
      sys.exit(1)
    
    auto.setServerInfo(options.webServer, options.httpPort, options.sslPort)
    sys.exit(mochitest.runTests(options))
    
if __name__ == "__main__":
  main()


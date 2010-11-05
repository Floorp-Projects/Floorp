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
import tempfile

sys.path.insert(0, os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0]))))

from automation import Automation
from remoteautomation import RemoteAutomation
from runtests import Mochitest
from runtests import MochitestOptions
from runtests import MochitestServer

import devicemanager

class RemoteOptions(MochitestOptions):

    def __init__(self, automation, scriptdir, **kwargs):
        defaults = {}
        MochitestOptions.__init__(self, automation, scriptdir)

        self.add_option("--remote-app-path", action="store",
                    type = "string", dest = "remoteAppPath",
                    help = "Path to remote executable relative to device root using only forward slashes. Either this or app must be specified but not both")
        defaults["remoteAppPath"] = None

        self.add_option("--deviceIP", action="store",
                    type = "string", dest = "deviceIP",
                    help = "ip address of remote device to test")
        defaults["deviceIP"] = None

        self.add_option("--devicePort", action="store",
                    type = "string", dest = "devicePort",
                    help = "port of remote device to test")
        defaults["devicePort"] = 20701

        self.add_option("--remote-product-name", action="store",
                    type = "string", dest = "remoteProductName",
                    help = "The executable's name of remote product to test - either fennec or firefox, defaults to fennec")
        defaults["remoteProductName"] = "fennec"

        self.add_option("--remote-logfile", action="store",
                    type = "string", dest = "remoteLogFile",
                    help = "Name of log file on the device relative to the device root.  PLEASE ONLY USE A FILENAME.")
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

        defaults["remoteTestRoot"] = None
        defaults["logFile"] = "mochitest.log"
        defaults["autorun"] = True
        defaults["closeWhenDone"] = True
        defaults["testPath"] = ""
        defaults["app"] = None

        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options, automation):
        options.remoteTestRoot = automation._devicemanager.getDeviceRoot()

        options.utilityPath = options.remoteTestRoot + "/bin"
        options.certPath = options.remoteTestRoot + "/certs"

       
        if options.remoteWebServer == None and os.name != "nt":
            options.remoteWebServer = automation.getLanIp()
        elif os.name == "nt":
            print "ERROR: you must specify a remoteWebServer ip address\n"
            return None

        options.webServer = options.remoteWebServer

        if (options.deviceIP == None):
            print "ERROR: you must provide a device IP"
            return None

        if (options.remoteLogFile == None):
            options.remoteLogFile = automation._devicemanager.getDeviceRoot() + '/test.log'

        if (options.remoteLogFile.count('/') < 1):
            options.remoteLogFile = automation._devicemanager.getDeviceRoot() + '/' + options.remoteLogFile

        # Set up our options that we depend on based on the above
        productRoot = options.remoteTestRoot + "/" + automation._product
        options.utilityPath = productRoot + "/bin"

        # remoteAppPath or app must be specified to find the product to launch
        if (options.remoteAppPath and options.app):
            print "ERROR: You cannot specify both the remoteAppPath and the app setting"
            return None
        elif (options.remoteAppPath):
            options.app = options.remoteTestRoot + "/" + options.remoteAppPath
        elif (options.app == None):
            # Neither remoteAppPath nor app are set -- error
            print "ERROR: You must specify either appPath or app"
            return None

        # Only reset the xrePath if it wasn't provided
        if (options.xrePath == None):
            if (automation._product == "fennec"):
                options.xrePath = productRoot + "/xulrunner"
            else:
                options.xrePath = options.utilityPath

        return options

    def verifyOptions(self, options, mochitest):
        # since we are reusing verifyOptions, it will exit if App is not found
        temp = options.app
        options.app = sys.argv[0]
        tempPort = options.httpPort
        tempSSL = options.sslPort
        tempIP = options.webServer
        options = MochitestOptions.verifyOptions(self, options, mochitest)
        options.webServer = tempIP
        options.app = temp
        options.sslPort = tempSSL
        options.httpPort = tempPort

        return options 

class MochiRemote(Mochitest):

    _automation = None
    _dm = None

    def __init__(self, automation, devmgr, options):
        self._automation = automation
        Mochitest.__init__(self, self._automation)
        self._dm = devmgr
        self.runSSLTunnel = False
        self.remoteProfile = options.remoteTestRoot + "/profile"
        self.remoteLog = options.remoteLogFile

    def cleanup(self, manifest, options):
        self._dm.getFile(self.remoteLog, self.localLog)
        self._dm.removeFile(self.remoteLog)
        self._dm.removeDir(self.remoteProfile)

    def findPath(self, paths, filename = None):
        for path in paths:
            p = path
            if filename:
                p = os.path.join(p, filename)
            if os.path.exists(self.getFullPath(p)):
                return path
        return None

    def startWebServer(self, options):
        """ Create the webserver on the host and start it up """
        remoteXrePath = options.xrePath
        remoteProfilePath = options.profilePath
        remoteUtilityPath = options.utilityPath
        localAutomation = Automation()

        paths = [options.xrePath, localAutomation.DIST_BIN, self._automation._product, os.path.join('..', self._automation._product)]
        options.xrePath = self.findPath(paths)
        if options.xrePath == None:
            print "ERROR: unable to find xulrunner path for %s, please specify with --xre-path" % (os.name)
            sys.exit(1)
        paths.append("bin")
        paths.append(os.path.join("..", "bin"))

        xpcshell = "xpcshell"
        if (os.name == "nt"):
            xpcshell += ".exe"
      
        if (options.utilityPath):
            paths.insert(0, options.utilityPath)
        options.utilityPath = self.findPath(paths, xpcshell)
        if options.utilityPath == None:
            print "ERROR: unable to find utility path for %s, please specify with --utility-path" % (os.name)
            sys.exit(1)

        options.profilePath = tempfile.mkdtemp()
        self.server = MochitestServer(localAutomation, options)
        self.server.start()

        self.server.ensureReady(self.SERVER_STARTUP_TIMEOUT)
        options.xrePath = remoteXrePath
        options.utilityPath = remoteUtilityPath
        options.profilePath = remoteProfilePath
         
    def stopWebServer(self, options):
        self.server.stop()
        
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
        options.profilePath = self.localProfile
        retVal = Mochitest.buildURLOptions(self, options)
        #we really need testConfig.js (for browser chrome)
        if self._dm.pushDir(options.profilePath, self.remoteProfile) == None:
            raise devicemanager.FileError("Unable to copy profile to device.")

        options.profilePath = self.remoteProfile
        options.logFile = self.localLog
        return retVal

    def installChromeFile(self, filename, options):
        parts = options.app.split('/')
        if (parts[0] == options.app):
          return "NO_CHROME_ON_DROID"
        path = '/'.join(parts[:-1])
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
    options = parser.verifyRemoteOptions(options, auto)
    if (options == None):
        print "ERROR: Invalid options specified, use --help for a list of valid options"
        sys.exit(1)

    productPieces = options.remoteProductName.split('.')
    if (productPieces != None):
        auto.setProduct(productPieces[0])
    else:
        auto.setProduct(options.remoteProductName)

    mochitest = MochiRemote(auto, dm, options)

    options = parser.verifyOptions(options, mochitest)
    if (options == None):
        sys.exit(1)
    
    auto.setRemoteLog(options.remoteLogFile)
    auto.setServerInfo(options.webServer, options.httpPort, options.sslPort)
    sys.exit(mochitest.runTests(options))
    
if __name__ == "__main__":
    main()


# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
import time
import tempfile
import re
import traceback
import shutil

sys.path.insert(0, os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0]))))

from automation import Automation
from remoteautomation import RemoteAutomation, fennecLogcatFilters
from runtests import Mochitest
from runtests import MochitestOptions
from runtests import MochitestServer

import devicemanager, devicemanagerADB, devicemanagerSUT
import manifestparser

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

        self.add_option("--dm_trans", action="store",
                    type = "string", dest = "dm_trans",
                    help = "the transport to use to communicate with device: [adb|sut]; default=sut")
        defaults["dm_trans"] = "sut"

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
                    help = "http port of the remote web server")
        defaults["httpPort"] = automation.DEFAULT_HTTP_PORT

        self.add_option("--ssl-port", action = "store",
                    type = "string", dest = "sslPort",
                    help = "ssl port of the remote web server")
        defaults["sslPort"] = automation.DEFAULT_SSL_PORT

        self.add_option("--pidfile", action = "store",
                    type = "string", dest = "pidFile",
                    help = "name of the pidfile to generate")
        defaults["pidFile"] = ""

        self.add_option("--robocop", action = "store",
                    type = "string", dest = "robocop",
                    help = "name of the .ini file containing the list of tests to run")
        defaults["robocop"] = ""

        self.add_option("--robocop-path", action = "store",
                    type = "string", dest = "robocopPath",
                    help = "Path to the folder where robocop.apk is located at.  Primarily used for ADB test running")
        defaults["robocopPath"] = ""

        self.add_option("--robocop-ids", action = "store",
                    type = "string", dest = "robocopIds",
                    help = "name of the file containing the view ID map (fennec_ids.txt)")
        defaults["robocopIds"] = ""

        self.add_option("--remoteTestRoot", action = "store",
                    type = "string", dest = "remoteTestRoot",
                    help = "remote directory to use as test root (eg. /mnt/sdcard/tests or /data/local/tests)")
        defaults["remoteTestRoot"] = None

        defaults["logFile"] = "mochitest.log"
        defaults["autorun"] = True
        defaults["closeWhenDone"] = True
        defaults["testPath"] = ""
        defaults["app"] = None

        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options, automation):
        if not options.remoteTestRoot:
            options.remoteTestRoot = automation._devicemanager.getDeviceRoot()
        productRoot = options.remoteTestRoot + "/" + automation._product

        if (options.utilityPath == self._automation.DIST_BIN):
            options.utilityPath = productRoot + "/bin"

        if options.remoteWebServer == None:
            if os.name != "nt":
                options.remoteWebServer = automation.getLanIp()
            else:
                print "ERROR: you must specify a --remote-webserver=<ip address>\n"
                return None

        options.webServer = options.remoteWebServer

        if (options.deviceIP == None):
            print "ERROR: you must provide a device IP"
            return None

        if (options.remoteLogFile == None):
            options.remoteLogFile = options.remoteTestRoot + '/logs/mochitest.log'

        if (options.remoteLogFile.count('/') < 1):
            options.remoteLogFile = options.remoteTestRoot + '/' + options.remoteLogFile

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

        if (options.pidFile != ""):
            f = open(options.pidFile, 'w')
            f.write("%s" % os.getpid())
            f.close()

        # Robocop specific options
        if options.robocop != "":
            if not os.path.exists(options.robocop):
                print "ERROR: Unable to find specified manifest '%s'" % options.robocop
                return None
            options.robocop = os.path.abspath(options.robocop)

        if options.robocopPath != "":
            if not os.path.exists(os.path.join(options.robocopPath, 'robocop.apk')):
                print "ERROR: Unable to find robocop.apk in path '%s'" % options.robocopPath
                return None
            options.robocopPath = os.path.abspath(options.robocopPath)

        if options.robocopIds != "":
            if not os.path.exists(options.robocopIds):
                print "ERROR: Unable to find specified IDs file '%s'" % options.robocopIds
                return None
            options.robocopIds = os.path.abspath(options.robocopIds)

        # allow us to keep original application around for cleanup while running robocop via 'am'
        options.remoteappname = options.app
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
    localProfile = None
    logLines = []

    def __init__(self, automation, devmgr, options):
        self._automation = automation
        Mochitest.__init__(self, self._automation)
        self._dm = devmgr
        self.runSSLTunnel = False
        self.remoteProfile = options.remoteTestRoot + "/profile"
        self._automation.setRemoteProfile(self.remoteProfile)
        self.remoteLog = options.remoteLogFile
        self.localLog = options.logFile

    def cleanup(self, manifest, options):
        if self._dm.fileExists(self.remoteLog):
            self._dm.getFile(self.remoteLog, self.localLog)
            self._dm.removeFile(self.remoteLog)
        else:
            print "WARNING: Unable to retrieve log file (%s) from remote " \
                "device" % self.remoteLog
        self._dm.removeDir(self.remoteProfile)

        if (options.pidFile != ""):
            try:
                os.remove(options.pidFile)
                os.remove(options.pidFile + ".xpcshell.pid")
            except:
                print "Warning: cleaning up pidfile '%s' was unsuccessful from the test harness" % options.pidFile

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
        localAutomation.IS_WIN32 = False
        localAutomation.IS_LINUX = False
        localAutomation.IS_MAC = False
        localAutomation.UNIXISH = False
        hostos = sys.platform
        if (hostos == 'mac' or  hostos == 'darwin'):
          localAutomation.IS_MAC = True
        elif (hostos == 'linux' or hostos == 'linux2'):
          localAutomation.IS_LINUX = True
          localAutomation.UNIXISH = True
        elif (hostos == 'win32' or hostos == 'win64'):
          localAutomation.BIN_SUFFIX = ".exe"
          localAutomation.IS_WIN32 = True

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

        if (options.pidFile != ""):
            f = open(options.pidFile + ".xpcshell.pid", 'w')
            f.write("%s" % self.server._process.pid)
            f.close()
        self.server.ensureReady(self.SERVER_STARTUP_TIMEOUT)

        options.xrePath = remoteXrePath
        options.utilityPath = remoteUtilityPath
        options.profilePath = remoteProfilePath
         
    def stopWebServer(self, options):
        self.server.stop()
        
    def buildProfile(self, options):
        if self.localProfile:
            options.profilePath = self.localProfile
        manifest = Mochitest.buildProfile(self, options)
        self.localProfile = options.profilePath
        self._dm.removeDir(self.remoteProfile)

        # we do not need this for robotium based tests, lets save a LOT of time
        if options.robocop:
            shutil.rmtree(os.path.join(options.profilePath, 'webapps'))
            shutil.rmtree(os.path.join(options.profilePath, 'extensions', 'staged', 'mochikit@mozilla.org'))
            shutil.rmtree(os.path.join(options.profilePath, 'extensions', 'staged', 'worker-test@mozilla.org'))
            shutil.rmtree(os.path.join(options.profilePath, 'extensions', 'staged', 'workerbootstrap-test@mozilla.org'))
            shutil.rmtree(os.path.join(options.profilePath, 'extensions', 'staged', 'special-powers@mozilla.org'))
            os.remove(os.path.join(options.profilePath, 'userChrome.css'))
            if os.path.exists(os.path.join(options.profilePath, 'tests.jar')):
                os.remove(os.path.join(options.profilePath, 'tests.jar'))
            if os.path.exists(os.path.join(options.profilePath, 'tests.manifest')):
                os.remove(os.path.join(options.profilePath, 'tests.manifest'))

        try:
            self._dm.pushDir(options.profilePath, self.remoteProfile)
        except devicemanager.DMError:
            print "Automation Error: Unable to copy profile to device."
            raise

        options.profilePath = self.remoteProfile
        return manifest
    
    def buildURLOptions(self, options, env):
        self.localLog = options.logFile
        options.logFile = self.remoteLog
        options.profilePath = self.localProfile
        env["MOZ_HIDE_RESULTS_TABLE"] = "1"
        retVal = Mochitest.buildURLOptions(self, options, env)

        if not options.robocop:
            #we really need testConfig.js (for browser chrome)
            try:
                self._dm.pushDir(options.profilePath, self.remoteProfile)
            except devicemanager.DMError:
                print "Automation Error: Unable to copy profile to device."
                raise

        options.profilePath = self.remoteProfile
        options.logFile = self.localLog
        return retVal

    def installChromeFile(self, filename, options):
        parts = options.app.split('/')
        if (parts[0] == options.app):
          return "NO_CHROME_ON_DROID"
        path = '/'.join(parts[:-1])
        manifest = path + "/chrome/" + os.path.basename(filename)
        try:
            self._dm.pushFile(filename, manifest)
        except devicemanager.DMError:
            print "Automation Error: Unable to install Chrome files on device."
            raise

        return manifest

    def getLogFilePath(self, logFile):             
        return logFile

    # In the future we could use LogParser: http://hg.mozilla.org/automation/logparser/
    def addLogData(self):
        with open(self.localLog) as currentLog:
            data = currentLog.readlines()

        restart = re.compile('0 INFO SimpleTest START.*')
        reend = re.compile('([0-9]+) INFO TEST-START . Shutdown.*')
        start_found = False
        end_found = False
        for line in data:
            if reend.match(line):
                end_found = True
                start_found = False
                return

            if start_found and not end_found:
                # Append the line without the number to increment
                self.logLines.append(' '.join(line.split(' ')[1:]))

            if restart.match(line):
                start_found = True

    def printLog(self):
        passed = 0
        failed = 0
        todo = 0
        incr = 1
        logFile = [] 
        logFile.append("0 INFO SimpleTest START")
        for line in self.logLines:
            if line.startswith("INFO TEST-PASS"):
                passed += 1
            elif line.startswith("INFO TEST-UNEXPECTED"):
                failed += 1
            elif line.startswith("INFO TEST-KNOWN"):
                todo += 1
            incr += 1

        logFile.append("%s INFO TEST-START | Shutdown" % incr)
        incr += 1
        logFile.append("%s INFO Passed: %s" % (incr, passed))
        incr += 1
        logFile.append("%s INFO Failed: %s" % (incr, failed))
        incr += 1
        logFile.append("%s INFO Todo: %s" % (incr, todo))
        incr += 1
        logFile.append("%s INFO SimpleTest FINISHED" % incr)

        # TODO: Consider not printing to stdout because we might be duplicating output
        print '\n'.join(logFile)
        with open(self.localLog, 'w') as localLog:
            localLog.write('\n'.join(logFile))

        if failed > 0:
            return 1
        return 0

    def printDeviceInfo(self):
        try:
            logcat = self._dm.getLogcat(filterOutRegexps=fennecLogcatFilters)
            print ''.join(logcat)
            print self._dm.getInfo()
        except devicemanager.DMError:
            print "WARNING: Error getting device information"

    def buildRobotiumConfig(self, options, browserEnv):
        deviceRoot = self._dm.getDeviceRoot()
        fHandle = tempfile.NamedTemporaryFile(suffix='.config',
                                              prefix='robotium-',
                                              dir=os.getcwd(),
                                              delete=False)
        fHandle.write("profile=%s\n" % (self.remoteProfile))
        fHandle.write("logfile=%s\n" % (options.remoteLogFile))
        fHandle.write("host=http://mochi.test:8888/tests\n")
        fHandle.write("rawhost=http://%s:%s/tests\n" % (options.remoteWebServer, options.httpPort))

        if browserEnv:
            envstr = ""
            delim = ""
            for key, value in browserEnv.items():
                try:
                    value.index(',')
                    print "Found: Error an ',' in our value, unable to process value."
                except ValueError, e:
                    envstr += "%s%s=%s" % (delim, key, value)
                    delim = ","

            fHandle.write("envvars=%s\n" % envstr)
        fHandle.close()

        self._dm.removeFile(os.path.join(deviceRoot, "robotium.config"))
        self._dm.pushFile(fHandle.name, os.path.join(deviceRoot, "robotium.config"))
        os.unlink(fHandle.name)

    def buildBrowserEnv(self, options):
        browserEnv = Mochitest.buildBrowserEnv(self, options)
        self.buildRobotiumConfig(options, browserEnv)
        return browserEnv

        
def main():
    scriptdir = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))
    auto = RemoteAutomation(None, "fennec")
    parser = RemoteOptions(auto, scriptdir)
    options, args = parser.parse_args()
    if (options.dm_trans == "adb"):
        if (options.deviceIP):
            dm = devicemanagerADB.DeviceManagerADB(options.deviceIP, options.devicePort, deviceRoot=options.remoteTestRoot)
        else:
            dm = devicemanagerADB.DeviceManagerADB(deviceRoot=options.remoteTestRoot)
    else:
         dm = devicemanagerSUT.DeviceManagerSUT(options.deviceIP, options.devicePort, deviceRoot=options.remoteTestRoot)
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
    
    logParent = os.path.dirname(options.remoteLogFile)
    dm.mkDir(logParent);
    auto.setRemoteLog(options.remoteLogFile)
    auto.setServerInfo(options.webServer, options.httpPort, options.sslPort)

    print dm.getInfo()

    procName = options.app.split('/')[-1]
    if (dm.processExist(procName)):
        dm.killProcess(procName)

    if options.robocop != "":
        mp = manifestparser.TestManifest(strict=False)
        # TODO: pull this in dynamically
        mp.read(options.robocop)
        robocop_tests = mp.active_tests(exists=False)

        deviceRoot = dm.getDeviceRoot()      
        dm.removeFile(os.path.join(deviceRoot, "fennec_ids.txt"))
        fennec_ids = os.path.abspath("fennec_ids.txt")
        if not os.path.exists(fennec_ids) and options.robocopIds:
            fennec_ids = options.robocopIds
        dm.pushFile(fennec_ids, os.path.join(deviceRoot, "fennec_ids.txt"))
        options.extraPrefs.append('robocop.logfile="%s/robocop.log"' % deviceRoot)
        options.extraPrefs.append('browser.search.suggest.enabled=true')
        options.extraPrefs.append('browser.search.suggest.prompted=true')

        if (options.dm_trans == 'adb' and options.robocopPath):
          dm._checkCmd(["install", "-r", os.path.join(options.robocopPath, "robocop.apk")])

        retVal = None
        for test in robocop_tests:
            if options.testPath and options.testPath != test['name']:
                continue

            options.app = "am"
            options.browserArgs = ["instrument", "-w", "-e", "deviceroot", deviceRoot, "-e", "class"]
            options.browserArgs.append("%s.tests.%s" % (options.remoteappname, test['name']))
            options.browserArgs.append("org.mozilla.roboexample.test/%s.FennecInstrumentationTestRunner" % options.remoteappname)

            try:
                dm.recordLogcat()
                result = mochitest.runTests(options)
                if result != 0:
                    print "ERROR: runTests() exited with code %s" % result
                    mochitest.printDeviceInfo()
                # Ensure earlier failures aren't overwritten by success on this run
                if retVal is None or retVal == 0:
                    retVal = result
                mochitest.addLogData()
            except:
                print "Automation Error: Exception caught while running tests"
                traceback.print_exc()
                mochitest.stopWebServer(options)
                mochitest.stopWebSocketServer(options)
                try:
                    mochitest.cleanup(None, options)
                except devicemanager.DMError:
                    # device error cleaning up... oh well!
                    pass
                retVal = 1
                break
        if retVal is None:
            print "No tests run. Did you pass an invalid TEST_PATH?"
            retVal = 1
        else:
            # if we didn't have some kind of error running the tests, make
            # sure the tests actually passed
            print "INFO | runtests.py | Test summary: start."
            overallResult = mochitest.printLog()
            print "INFO | runtests.py | Test summary: end."
            if retVal == 0:
                retVal = overallResult
    else:
        try:
            dm.recordLogcat()
            retVal = mochitest.runTests(options)
        except:
            print "Automation Error: Exception caught while running tests"
            traceback.print_exc()
            mochitest.stopWebServer(options)
            mochitest.stopWebSocketServer(options)
            try:
                mochitest.cleanup(None, options)
            except devicemanager.DMError:
                # device error cleaning up... oh well!
                pass
            retVal = 1

    mochitest.printDeviceInfo()

    sys.exit(retVal)

if __name__ == "__main__":
    main()

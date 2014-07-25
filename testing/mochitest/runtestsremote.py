# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import base64
import json
import math
import os
import re
import shutil
import sys
import tempfile
import traceback

sys.path.insert(0, os.path.abspath(os.path.realpath(os.path.dirname(__file__))))

from automation import Automation
from remoteautomation import RemoteAutomation, fennecLogcatFilters
from runtests import Mochitest, MessageLogger, MochitestFormatter
from mochitest_options import MochitestOptions

import devicemanager
import droid
import manifestparser
import mozinfo
import moznetwork
from mozlog.structured.handlers import StreamHandler
from mozlog.structured.structuredlog import StructuredLogger

log = StructuredLogger('Mochi-Remote')
stream_handler = StreamHandler(stream=sys.stdout, formatter=MochitestFormatter())
log.add_handler(stream_handler)

SCRIPT_DIR = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))

class RemoteOptions(MochitestOptions):

    def __init__(self, automation, **kwargs):
        defaults = {}
        self._automation = automation or Automation()
        MochitestOptions.__init__(self)

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

        self.add_option("--robocop-ini", action = "store",
                    type = "string", dest = "robocopIni",
                    help = "name of the .ini file containing the list of tests to run")
        defaults["robocopIni"] = ""

        self.add_option("--robocop", action = "store",
                    type = "string", dest = "robocop",
                    help = "name of the .ini file containing the list of tests to run. [DEPRECATED- please use --robocop-ini")
        defaults["robocop"] = ""

        self.add_option("--robocop-apk", action = "store",
                    type = "string", dest = "robocopApk",
                    help = "name of the Robocop APK to use for ADB test running")
        defaults["robocopApk"] = ""

        self.add_option("--robocop-path", action = "store",
                    type = "string", dest = "robocopPath",
                    help = "Path to the folder where robocop.apk is located at.  Primarily used for ADB test running. [DEPRECATED- please use --robocop-apk]")
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
        defaults["utilityPath"] = None

        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options, automation):
        if not options.remoteTestRoot:
            options.remoteTestRoot = automation._devicemanager.deviceRoot

        if options.remoteWebServer == None:
            if os.name != "nt":
                options.remoteWebServer = moznetwork.get_ip()
            else:
                log.error("you must specify a --remote-webserver=<ip address>")
                return None

        options.webServer = options.remoteWebServer

        if (options.deviceIP == None):
            log.error("you must provide a device IP")
            return None

        if (options.remoteLogFile == None):
            options.remoteLogFile = options.remoteTestRoot + '/logs/mochitest.log'

        if (options.remoteLogFile.count('/') < 1):
            options.remoteLogFile = options.remoteTestRoot + '/' + options.remoteLogFile

        # remoteAppPath or app must be specified to find the product to launch
        if (options.remoteAppPath and options.app):
            log.error("You cannot specify both the remoteAppPath and the app setting")
            return None
        elif (options.remoteAppPath):
            options.app = options.remoteTestRoot + "/" + options.remoteAppPath
        elif (options.app == None):
            # Neither remoteAppPath nor app are set -- error
            log.error("You must specify either appPath or app")
            return None

        # Only reset the xrePath if it wasn't provided
        if (options.xrePath == None):
            options.xrePath = options.utilityPath

        if (options.pidFile != ""):
            f = open(options.pidFile, 'w')
            f.write("%s" % os.getpid())
            f.close()

        # Robocop specific deprecated options.
        if options.robocop:
            if options.robocopIni:
                log.error("can not use deprecated --robocop and replacement --robocop-ini together")
                return None
            options.robocopIni = options.robocop
            del options.robocop

        if options.robocopPath:
            if options.robocopApk:
                log.error("can not use deprecated --robocop-path and replacement --robocop-apk together")
                return None
            options.robocopApk = os.path.join(options.robocopPath, 'robocop.apk')
            del options.robocopPath

        # Robocop specific options
        if options.robocopIni != "":
            if not os.path.exists(options.robocopIni):
                log.error("Unable to find specified robocop .ini manifest '%s'" % options.robocopIni)
                return None
            options.robocopIni = os.path.abspath(options.robocopIni)

        if options.robocopApk != "":
            if not os.path.exists(options.robocopApk):
                log.error("Unable to find robocop APK '%s'" % options.robocopApk)
                return None
            options.robocopApk = os.path.abspath(options.robocopApk)

        if options.robocopIds != "":
            if not os.path.exists(options.robocopIds):
                log.error("Unable to find specified robocop IDs file '%s'" % options.robocopIds)
                return None
            options.robocopIds = os.path.abspath(options.robocopIds)

        # allow us to keep original application around for cleanup while running robocop via 'am'
        options.remoteappname = options.app
        return options

    def verifyOptions(self, options, mochitest):
        # since we are reusing verifyOptions, it will exit if App is not found
        temp = options.app
        options.app = __file__
        tempPort = options.httpPort
        tempSSL = options.sslPort
        tempIP = options.webServer
        # We are going to override this option later anyway, just pretend
        # like it's not set for verification purposes.
        options.dumpOutputDirectory = None
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
    logMessages = []

    def __init__(self, automation, devmgr, options, message_logger=None):
        self._automation = automation
        Mochitest.__init__(self)
        self._dm = devmgr
        self.environment = self._automation.environment
        self.remoteProfile = options.remoteTestRoot + "/profile"
        self._automation.setRemoteProfile(self.remoteProfile)
        self.remoteLog = options.remoteLogFile
        self.localLog = options.logFile
        self._automation.deleteANRs()
        self.certdbNew = True

        # structured logging
        self.message_logger = message_logger or MessageLogger(logger=log)

    def cleanup(self, options):
        if self._dm.fileExists(self.remoteLog):
            self._dm.getFile(self.remoteLog, self.localLog)
            self._dm.removeFile(self.remoteLog)
        else:
            log.warning("Unable to retrieve log file (%s) from remote device" % self.remoteLog)
        self._dm.removeDir(self.remoteProfile)
        Mochitest.cleanup(self, options)

    def findPath(self, paths, filename = None):
        for path in paths:
            p = path
            if filename:
                p = os.path.join(p, filename)
            if os.path.exists(self.getFullPath(p)):
                return path
        return None

    def makeLocalAutomation(self):
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
        return localAutomation

    # This seems kludgy, but this class uses paths from the remote host in the
    # options, except when calling up to the base class, which doesn't
    # understand the distinction.  This switches out the remote values for local
    # ones that the base class understands.  This is necessary for the web
    # server, SSL tunnel and profile building functions.
    def switchToLocalPaths(self, options):
        """ Set local paths in the options, return a function that will restore remote values """
        remoteXrePath = options.xrePath
        remoteProfilePath = options.profilePath
        remoteUtilityPath = options.utilityPath

        localAutomation = self.makeLocalAutomation()
        paths = [
            options.xrePath,
            localAutomation.DIST_BIN,
            self._automation._product,
            os.path.join('..', self._automation._product)
        ]
        options.xrePath = self.findPath(paths)
        if options.xrePath == None:
            log.error("unable to find xulrunner path for %s, please specify with --xre-path" % os.name)
            sys.exit(1)

        xpcshell = "xpcshell"
        if (os.name == "nt"):
            xpcshell += ".exe"

        if options.utilityPath:
            paths = [options.utilityPath, options.xrePath]
        else:
            paths = [options.xrePath]
        options.utilityPath = self.findPath(paths, xpcshell)

        if options.utilityPath == None:
            log.error("unable to find utility path for %s, please specify with --utility-path" % os.name)
            sys.exit(1)

        xpcshell_path = os.path.join(options.utilityPath, xpcshell)
        if localAutomation.elf_arm(xpcshell_path):
            log.error('xpcshell at %s is an ARM binary; please use '
                      'the --utility-path argument to specify the path '
                      'to a desktop version.' % xpcshell_path)
            sys.exit(1)

        if self.localProfile:
            options.profilePath = self.localProfile
        else:
            options.profilePath = None

        def fixup():
            options.xrePath = remoteXrePath
            options.utilityPath = remoteUtilityPath
            options.profilePath = remoteProfilePath

        return fixup

    def startServers(self, options, debuggerInfo):
        """ Create the servers on the host and start them up """
        restoreRemotePaths = self.switchToLocalPaths(options)
        Mochitest.startServers(self, options, debuggerInfo)
        restoreRemotePaths()

    def buildProfile(self, options):
        restoreRemotePaths = self.switchToLocalPaths(options)
        manifest = Mochitest.buildProfile(self, options)
        self.localProfile = options.profilePath
        self._dm.removeDir(self.remoteProfile)

        # we do not need this for robotium based tests, lets save a LOT of time
        if options.robocopIni:
            shutil.rmtree(os.path.join(options.profilePath, 'webapps'))
            shutil.rmtree(os.path.join(options.profilePath, 'extensions', 'staged', 'mochikit@mozilla.org'))
            shutil.rmtree(os.path.join(options.profilePath, 'extensions', 'staged', 'worker-test@mozilla.org'))
            shutil.rmtree(os.path.join(options.profilePath, 'extensions', 'staged', 'workerbootstrap-test@mozilla.org'))
            os.remove(os.path.join(options.profilePath, 'userChrome.css'))

        try:
            self._dm.pushDir(options.profilePath, self.remoteProfile)
        except devicemanager.DMError:
            log.error("Automation Error: Unable to copy profile to device.")
            raise

        restoreRemotePaths()
        options.profilePath = self.remoteProfile
        return manifest

    def buildURLOptions(self, options, env):
        self.localLog = options.logFile
        options.logFile = self.remoteLog
        options.profilePath = self.localProfile
        env["MOZ_HIDE_RESULTS_TABLE"] = "1"
        retVal = Mochitest.buildURLOptions(self, options, env)

        if not options.robocopIni:
            #we really need testConfig.js (for browser chrome)
            try:
                self._dm.pushDir(options.profilePath, self.remoteProfile)
            except devicemanager.DMError:
                log.error("Automation Error: Unable to copy profile to device.")
                raise

        options.profilePath = self.remoteProfile
        options.logFile = self.localLog
        return retVal

    def getTestsToRun(self, options):
        if options.robocopIni != "":
            # Skip over tests filtering if we run robocop tests.
            return None
        else:
            return super(MochiRemote, self).getTestsToRun(options)

    def buildTestPath(self, options, testsToFilter=None):
        if options.robocopIni != "":
            # Skip over manifest building if we just want to run
            # robocop tests.
            return self.buildTestURL(options)
        else:
            return super(MochiRemote, self).buildTestPath(options, testsToFilter)

    def installChromeFile(self, filename, options):
        parts = options.app.split('/')
        if (parts[0] == options.app):
          return "NO_CHROME_ON_DROID"
        path = '/'.join(parts[:-1])
        manifest = path + "/chrome/" + os.path.basename(filename)
        try:
            self._dm.pushFile(filename, manifest)
        except devicemanager.DMError:
            log.error("Automation Error: Unable to install Chrome files on device.")
            raise

        return manifest

    def getLogFilePath(self, logFile):
        return logFile

    # In the future we could use LogParser: http://hg.mozilla.org/automation/logparser/
    def addLogData(self):
        with open(self.localLog) as currentLog:
            data = currentLog.readlines()
        start_found = False
        end_found = False
        fail_found = False
        for line in data:
            try:
                message = json.loads(line)
                if not isinstance(message, dict) or not 'action' in message:
                    continue
            except ValueError:
                continue

            if message['action'] == 'test_end':
                end_found = True
                start_found = False
                break

            if start_found and not end_found:
                self.logMessages.append(message)

            if message['action'] == 'test_start':
                start_found = True
            if 'expected' in message:
                fail_found = True
        result = 0
        if fail_found:
            result = 1
        if not end_found:
            log.error("Automation Error: Missing end of test marker (process crashed?)")
            result = 1
        return result

    def printLog(self):
        passed = 0
        failed = 0
        todo = 0
        incr = 1
        logFile = []
        logFile.append("0 INFO SimpleTest START")
        for message in self.logMessages:
            if 'status' not in message:
                continue

            if 'expected' in message:
                failed += 1
            elif message['status'] == 'PASS':
                passed += 1
            elif message['status'] == 'FAIL':
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

    def printScreenshots(self, screenShotDir):
        # TODO: This can be re-written after completion of bug 749421
        if not self._dm.dirExists(screenShotDir):
            log.info("SCREENSHOT: No ScreenShots directory available: " + screenShotDir)
            return

        printed = 0
        for name in self._dm.listFiles(screenShotDir):
            fullName = screenShotDir + "/" + name
            log.info("SCREENSHOT: FOUND: [%s]" % fullName)
            try:
                image = self._dm.pullFile(fullName)
                encoded = base64.b64encode(image)
                log.info("SCREENSHOT: data:image/jpg;base64,%s" % encoded)
                printed += 1
            except:
                log.info("SCREENSHOT: Could not be parsed")
                pass

        log.info("SCREENSHOT: TOTAL PRINTED: [%s]" % printed)

    def printDeviceInfo(self, printLogcat=False):
        try:
            if printLogcat:
                logcat = self._dm.getLogcat(filterOutRegexps=fennecLogcatFilters)
                log.info('\n' + ''.join(logcat).decode('utf-8', 'replace'))
            log.info("Device info: %s" % self._dm.getInfo())
            log.info("Test root: %s" % self._dm.deviceRoot)
        except devicemanager.DMError:
            log.warn("Error getting device information")

    def buildRobotiumConfig(self, options, browserEnv):
        deviceRoot = self._dm.deviceRoot
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
                    log.error("buildRobotiumConfig: browserEnv - Found a ',' in our value, unable to process value. key=%s,value=%s" % (key, value))
                    log.error("browserEnv=%s" % browserEnv)
                except ValueError:
                    envstr += "%s%s=%s" % (delim, key, value)
                    delim = ","

            fHandle.write("envvars=%s\n" % envstr)
        fHandle.close()

        self._dm.removeFile(os.path.join(deviceRoot, "robotium.config"))
        self._dm.pushFile(fHandle.name, os.path.join(deviceRoot, "robotium.config"))
        os.unlink(fHandle.name)

    def getGMPPluginPath(self, options):
        # TODO: bug 1043403
        return None

    def buildBrowserEnv(self, options, debugger=False):
        browserEnv = Mochitest.buildBrowserEnv(self, options, debugger=debugger)
        self.buildRobotiumConfig(options, browserEnv)
        return browserEnv

    def runApp(self, *args, **kwargs):
        """front-end automation.py's `runApp` functionality until FennecRunner is written"""

        # automation.py/remoteautomation `runApp` takes the profile path,
        # whereas runtest.py's `runApp` takes a mozprofile object.
        if 'profileDir' not in kwargs and 'profile' in kwargs:
            kwargs['profileDir'] = kwargs.pop('profile').profile

        # We're handling ssltunnel, so we should lie to automation.py to avoid
        # it trying to set up ssltunnel as well
        kwargs['runSSLTunnel'] = False

        if 'quiet' in kwargs:
            kwargs.pop('quiet')

        return self._automation.runApp(*args, **kwargs)

class RobocopMochiRemote(MochiRemote):
    """This class maintains compatibility with the robocop logging system
    that is still unstructured."""

    def addLogData(self):
        with open(self.localLog) as currentLog:
            data = currentLog.readlines()

        restart = re.compile('SimpleTest START.*')
        reend = re.compile('TEST-START . Shutdown.*')
        refail = re.compile('TEST-UNEXPECTED-FAIL.*')
        start_found = False
        end_found = False
        fail_found = False
        for line in data:
            if reend.match(line):
                end_found = True
                start_found = False
                break

            if start_found and not end_found:
                self.logMessages.append(line)

            if restart.match(line):
                start_found = True
            if refail.match(line):
                fail_found = True
        result = 0
        if fail_found:
            result = 1
        if not end_found:
            log.error("Automation Error: Missing end of test marker (process crashed?)")
            result = 1
        return result

    def printLog(self):
        passed = 0
        failed = 0
        todo = 0
        incr = 1
        logFile = []
        logFile.append("0 INFO SimpleTest START")
        for line in self.logMessages:
            if line.startswith("TEST-PASS"):
                passed += 1
            elif line.startswith("TEST-UNEXPECTED"):
                failed += 1
            elif line.startswith("TEST-KNOWN"):
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

def main():
    message_logger = MessageLogger(logger=log)
    process_args = {'messageLogger': message_logger}
    auto = RemoteAutomation(None, "fennec", processArgs=process_args)
    parser = RemoteOptions(auto)
    options, args = parser.parse_args()

    if (options.dm_trans == "adb"):
        if (options.deviceIP):
            dm = droid.DroidADB(options.deviceIP, options.devicePort, deviceRoot=options.remoteTestRoot)
        else:
            dm = droid.DroidADB(deviceRoot=options.remoteTestRoot)
    else:
         dm = droid.DroidSUT(options.deviceIP, options.devicePort, deviceRoot=options.remoteTestRoot)
    auto.setDeviceManager(dm)
    options = parser.verifyRemoteOptions(options, auto)
    if (options == None):
        log.error("Invalid options specified, use --help for a list of valid options")
        sys.exit(1)

    productPieces = options.remoteProductName.split('.')
    if (productPieces != None):
        auto.setProduct(productPieces[0])
    else:
        auto.setProduct(options.remoteProductName)
    auto.setAppName(options.remoteappname)

    mochitest_cls = RobocopMochiRemote if options.robocopIni != "" else MochiRemote
    mochitest = mochitest_cls(auto, dm, options, message_logger)

    options = parser.verifyOptions(options, mochitest)
    if (options == None):
        sys.exit(1)

    logParent = os.path.dirname(options.remoteLogFile)
    dm.mkDir(logParent);
    auto.setRemoteLog(options.remoteLogFile)
    auto.setServerInfo(options.webServer, options.httpPort, options.sslPort)

    mochitest.printDeviceInfo()

    # Add Android version (SDK level) to mozinfo so that manifest entries
    # can be conditional on android_version.
    androidVersion = dm.shellCheckOutput(['getprop', 'ro.build.version.sdk'])
    log.info("Android sdk version '%s'; will use this to filter manifests" % str(androidVersion))
    mozinfo.info['android_version'] = androidVersion

    deviceRoot = dm.deviceRoot
    if options.dmdPath:
        dmdLibrary = "libdmd.so"
        dmdPathOnDevice = os.path.join(deviceRoot, dmdLibrary)
        dm.removeFile(dmdPathOnDevice)
        dm.pushFile(os.path.join(options.dmdPath, dmdLibrary), dmdPathOnDevice)
        options.dmdPath = deviceRoot

    options.dumpOutputDirectory = deviceRoot

    procName = options.app.split('/')[-1]
    dm.killProcess(procName)

    if options.robocopIni != "":
        # sut may wait up to 300 s for a robocop am process before returning
        dm.default_timeout = 320
        mp = manifestparser.TestManifest(strict=False)
        # TODO: pull this in dynamically
        mp.read(options.robocopIni)
        robocop_tests = mp.active_tests(exists=False, **mozinfo.info)
        tests = []
        my_tests = tests
        for test in robocop_tests:
            tests.append(test['name'])
        # suite_start message when running robocop tests
        log.suite_start(tests)

        if options.totalChunks:
            tests_per_chunk = math.ceil(len(tests) / (options.totalChunks * 1.0))
            start = int(round((options.thisChunk-1) * tests_per_chunk))
            end = int(round(options.thisChunk * tests_per_chunk))
            if end > len(tests):
                end = len(tests)
            my_tests = tests[start:end]
            log.info("Running tests %d-%d/%d" % (start+1, end, len(tests)))

        dm.removeFile(os.path.join(deviceRoot, "fennec_ids.txt"))
        fennec_ids = os.path.abspath(os.path.join(SCRIPT_DIR, "fennec_ids.txt"))
        if not os.path.exists(fennec_ids) and options.robocopIds:
            fennec_ids = options.robocopIds
        dm.pushFile(fennec_ids, os.path.join(deviceRoot, "fennec_ids.txt"))
        options.extraPrefs.append('browser.search.suggest.enabled=true')
        options.extraPrefs.append('browser.search.suggest.prompted=true')
        options.extraPrefs.append('layout.css.devPixelsPerPx=1.0')
        options.extraPrefs.append('browser.chrome.dynamictoolbar=false')
        options.extraPrefs.append('browser.snippets.enabled=false')

        if (options.dm_trans == 'adb' and options.robocopApk):
            dm._checkCmd(["install", "-r", options.robocopApk])

        retVal = None
        for test in robocop_tests:
            if options.testPath and options.testPath != test['name']:
                continue

            if not test['name'] in my_tests:
                continue

            if 'disabled' in test:
                log.info('TEST-INFO | skipping %s | %s' % (test['name'], test['disabled']))
                continue

            # When running in a loop, we need to create a fresh profile for each cycle
            if mochitest.localProfile:
                options.profilePath = mochitest.localProfile
                os.system("rm -Rf %s" % options.profilePath)
                options.profilePath = None
                mochitest.localProfile = options.profilePath

            options.app = "am"
            options.browserArgs = ["instrument", "-w", "-e", "deviceroot", deviceRoot, "-e", "class"]
            options.browserArgs.append("org.mozilla.gecko.tests.%s" % test['name'])
            options.browserArgs.append("org.mozilla.roboexample.test/org.mozilla.gecko.FennecInstrumentationTestRunner")

            # If the test is for checking the import from bookmarks then make sure there is data to import
            if test['name'] == "testImportFromAndroid":

                # Get the OS so we can run the insert in the apropriate database and following the correct table schema
                osInfo = dm.getInfo("os")
                devOS = " ".join(osInfo['os'])

                if ("pandaboard" in devOS):
                    delete = ['execsu', 'sqlite3', "/data/data/com.android.browser/databases/browser2.db \'delete from bookmarks where _id > 14;\'"]
                else:
                    delete = ['execsu', 'sqlite3', "/data/data/com.android.browser/databases/browser.db \'delete from bookmarks where _id > 14;\'"]
                if (options.dm_trans == "sut"):
                    dm._runCmds([{"cmd": " ".join(delete)}])

                # Insert the bookmarks
                log.info("Insert bookmarks in the default android browser database")
                for i in range(20):
                   if ("pandaboard" in devOS):
                       cmd = ['execsu', 'sqlite3', "/data/data/com.android.browser/databases/browser2.db 'insert or replace into bookmarks(_id,title,url,folder,parent,position) values (" + str(30 + i) + ",\"Bookmark"+ str(i) + "\",\"http://www.bookmark" + str(i) + ".com\",0,1," + str(100 + i) + ");'"]
                   else:
                       cmd = ['execsu', 'sqlite3', "/data/data/com.android.browser/databases/browser.db 'insert into bookmarks(title,url,bookmark) values (\"Bookmark"+ str(i) + "\",\"http://www.bookmark" + str(i) + ".com\",1);'"]
                   if (options.dm_trans == "sut"):
                       dm._runCmds([{"cmd": " ".join(cmd)}])
            try:
                screenShotDir = "/mnt/sdcard/Robotium-Screenshots"
                dm.removeDir(screenShotDir)
                dm.recordLogcat()
                result = mochitest.runTests(options)
                if result != 0:
                    log.error("runTests() exited with code %s" % result)
                log_result = mochitest.addLogData()
                if result != 0 or log_result != 0:
                    mochitest.printDeviceInfo(printLogcat=True)
                    mochitest.printScreenshots(screenShotDir)
                # Ensure earlier failures aren't overwritten by success on this run
                if retVal is None or retVal == 0:
                    retVal = result
            except:
                log.error("Automation Error: Exception caught while running tests")
                traceback.print_exc()
                mochitest.stopServers()
                try:
                    mochitest.cleanup(options)
                except devicemanager.DMError:
                    # device error cleaning up... oh well!
                    pass
                retVal = 1
                break
            finally:
                # Clean-up added bookmarks
                if test['name'] == "testImportFromAndroid":
                    if ("pandaboard" in devOS):
                        cmd_del = ['execsu', 'sqlite3', "/data/data/com.android.browser/databases/browser2.db \'delete from bookmarks where _id > 14;\'"]
                    else:
                        cmd_del = ['execsu', 'sqlite3', "/data/data/com.android.browser/databases/browser.db \'delete from bookmarks where _id > 14;\'"]
                    if (options.dm_trans == "sut"):
                        dm._runCmds([{"cmd": " ".join(cmd_del)}])
        if retVal is None:
            log.warning("No tests run. Did you pass an invalid TEST_PATH?")
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
            log.error("Automation Error: Exception caught while running tests")
            traceback.print_exc()
            mochitest.stopServers()
            try:
                mochitest.cleanup(options)
            except devicemanager.DMError:
                # device error cleaning up... oh well!
                pass
            retVal = 1

    mochitest.printDeviceInfo(printLogcat=True)

    sys.exit(retVal)

if __name__ == "__main__":
    main()

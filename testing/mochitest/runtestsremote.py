# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import base64
import json
import os
import shutil
import sys
import tempfile
import traceback

sys.path.insert(
    0, os.path.abspath(
        os.path.realpath(
            os.path.dirname(__file__))))

from automation import Automation
from remoteautomation import RemoteAutomation, fennecLogcatFilters
from runtests import Mochitest, MessageLogger
from mochitest_options import MochitestArgumentParser

from manifestparser import TestManifest
from manifestparser.filters import chunk_by_slice
import devicemanager
import mozinfo

SCRIPT_DIR = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))


class MochiRemote(Mochitest):
    _automation = None
    _dm = None
    localProfile = None
    logMessages = []

    def __init__(self, automation, devmgr, options):
        Mochitest.__init__(self, options)

        self._automation = automation
        self._dm = devmgr
        self.environment = self._automation.environment
        self.remoteProfile = options.remoteTestRoot + "/profile"
        self._automation.setRemoteProfile(self.remoteProfile)
        self.remoteLog = options.remoteLogFile
        self.localLog = options.logFile
        self._automation.deleteANRs()
        self._automation.deleteTombstones()
        self.certdbNew = True
        self.remoteNSPR = os.path.join(options.remoteTestRoot, "nspr")
        self._dm.removeDir(self.remoteNSPR)
        self._dm.mkDir(self.remoteNSPR)
        self.remoteChromeTestDir = os.path.join(
            options.remoteTestRoot,
            "chrome")
        self._dm.removeDir(self.remoteChromeTestDir)
        self._dm.mkDir(self.remoteChromeTestDir)

    def cleanup(self, options):
        if self._dm.fileExists(self.remoteLog):
            self._dm.getFile(self.remoteLog, self.localLog)
            self._dm.removeFile(self.remoteLog)
        else:
            self.log.warning(
                "Unable to retrieve log file (%s) from remote device" %
                self.remoteLog)
        self._dm.removeDir(self.remoteProfile)
        self._dm.removeDir(self.remoteChromeTestDir)
        # Don't leave an old robotium.config hanging around; the
        # profile it references was just deleted!
        deviceRoot = self._dm.getDeviceRoot()
        self._dm.removeFile(os.path.join(deviceRoot, "robotium.config"))
        blobberUploadDir = os.environ.get('MOZ_UPLOAD_DIR', None)
        if blobberUploadDir:
            self._dm.getDirectory(self.remoteNSPR, blobberUploadDir)
        Mochitest.cleanup(self, options)

    def findPath(self, paths, filename=None):
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
        if (hostos == 'mac' or hostos == 'darwin'):
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
        if options.xrePath is None:
            self.log.error(
                "unable to find xulrunner path for %s, please specify with --xre-path" %
                os.name)
            sys.exit(1)

        xpcshell = "xpcshell"
        if (os.name == "nt"):
            xpcshell += ".exe"

        if options.utilityPath:
            paths = [options.utilityPath, options.xrePath]
        else:
            paths = [options.xrePath]
        options.utilityPath = self.findPath(paths, xpcshell)

        if options.utilityPath is None:
            self.log.error(
                "unable to find utility path for %s, please specify with --utility-path" %
                os.name)
            sys.exit(1)

        xpcshell_path = os.path.join(options.utilityPath, xpcshell)
        if localAutomation.elf_arm(xpcshell_path):
            self.log.error('xpcshell at %s is an ARM binary; please use '
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
        # ignoreSSLTunnelExts is a workaround for bug 1109310
        Mochitest.startServers(
            self,
            options,
            debuggerInfo,
            ignoreSSLTunnelExts=True)
        restoreRemotePaths()

    def buildProfile(self, options):
        restoreRemotePaths = self.switchToLocalPaths(options)
        manifest = Mochitest.buildProfile(self, options)
        self.localProfile = options.profilePath
        self._dm.removeDir(self.remoteProfile)

        # we do not need this for robotium based tests, lets save a LOT of time
        if options.robocopIni:
            shutil.rmtree(os.path.join(options.profilePath, 'webapps'))
            shutil.rmtree(
                os.path.join(
                    options.profilePath,
                    'extensions',
                    'staged',
                    'mochikit@mozilla.org'))
            shutil.rmtree(
                os.path.join(
                    options.profilePath,
                    'extensions',
                    'staged',
                    'worker-test@mozilla.org'))
            shutil.rmtree(
                os.path.join(
                    options.profilePath,
                    'extensions',
                    'staged',
                    'workerbootstrap-test@mozilla.org'))
            os.remove(os.path.join(options.profilePath, 'userChrome.css'))

        try:
            self._dm.pushDir(options.profilePath, self.remoteProfile)
        except devicemanager.DMError:
            self.log.error(
                "Automation Error: Unable to copy profile to device.")
            raise

        restoreRemotePaths()
        options.profilePath = self.remoteProfile
        return manifest

    def buildURLOptions(self, options, env):
        self.localLog = options.logFile
        options.logFile = self.remoteLog
        options.fileLevel = 'INFO'
        options.profilePath = self.localProfile
        env["MOZ_HIDE_RESULTS_TABLE"] = "1"
        retVal = Mochitest.buildURLOptions(self, options, env)

        if not options.robocopIni:
            # we really need testConfig.js (for browser chrome)
            try:
                self._dm.pushDir(options.profilePath, self.remoteProfile)
            except devicemanager.DMError:
                self.log.error(
                    "Automation Error: Unable to copy profile to device.")
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
            return super(
                MochiRemote,
                self).buildTestPath(
                options,
                testsToFilter)

    def getChromeTestDir(self, options):
        local = super(MochiRemote, self).getChromeTestDir(options)
        local = os.path.join(local, "chrome")
        remote = self.remoteChromeTestDir
        if options.chrome:
            self.log.info("pushing %s to %s on device..." % (local, remote))
            self._dm.pushDir(local, remote)
        return remote

    def getLogFilePath(self, logFile):
        return logFile

    # In the future we could use LogParser:
    # http://hg.mozilla.org/automation/logparser/
    def addLogData(self):
        with open(self.localLog) as currentLog:
            data = currentLog.readlines()
        start_found = False
        end_found = False
        fail_found = False
        for line in data:
            try:
                message = json.loads(line)
                if not isinstance(message, dict) or 'action' not in message:
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
            self.log.info(
                "PROCESS-CRASH | Automation Error: Missing end of test marker (process crashed?)")
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

        # TODO: Consider not printing to stdout because we might be duplicating
        # output
        print '\n'.join(logFile)
        with open(self.localLog, 'w') as localLog:
            localLog.write('\n'.join(logFile))

        if failed > 0:
            return 1
        return 0

    def printScreenshots(self, screenShotDir):
        # TODO: This can be re-written after completion of bug 749421
        if not self._dm.dirExists(screenShotDir):
            self.log.info(
                "SCREENSHOT: No ScreenShots directory available: " +
                screenShotDir)
            return

        printed = 0
        for name in self._dm.listFiles(screenShotDir):
            fullName = screenShotDir + "/" + name
            self.log.info("SCREENSHOT: FOUND: [%s]" % fullName)
            try:
                image = self._dm.pullFile(fullName)
                encoded = base64.b64encode(image)
                self.log.info("SCREENSHOT: data:image/jpg;base64,%s" % encoded)
                printed += 1
            except:
                self.log.info("SCREENSHOT: Could not be parsed")
                pass

        self.log.info("SCREENSHOT: TOTAL PRINTED: [%s]" % printed)

    def printDeviceInfo(self, printLogcat=False):
        try:
            if printLogcat:
                logcat = self._dm.getLogcat(
                    filterOutRegexps=fennecLogcatFilters)
                self.log.info(
                    '\n' +
                    ''.join(logcat).decode(
                        'utf-8',
                        'replace'))
            self.log.info("Device info:")
            devinfo = self._dm.getInfo()
            for category in devinfo:
                if type(devinfo[category]) is list:
                    self.log.info("  %s:" % category)
                    for item in devinfo[category]:
                        self.log.info("     %s" % item)
                else:
                    self.log.info("  %s: %s" % (category, devinfo[category]))
            self.log.info("Test root: %s" % self._dm.deviceRoot)
        except devicemanager.DMError:
            self.log.warning("Error getting device information")

    def buildRobotiumConfig(self, options, browserEnv):
        deviceRoot = self._dm.deviceRoot
        fHandle = tempfile.NamedTemporaryFile(suffix='.config',
                                              prefix='robotium-',
                                              dir=os.getcwd(),
                                              delete=False)
        fHandle.write("profile=%s\n" % (self.remoteProfile))
        fHandle.write("logfile=%s\n" % (options.remoteLogFile))
        fHandle.write("host=http://mochi.test:8888/tests\n")
        fHandle.write(
            "rawhost=http://%s:%s/tests\n" %
            (options.remoteWebServer, options.httpPort))

        if browserEnv:
            envstr = ""
            delim = ""
            for key, value in browserEnv.items():
                try:
                    value.index(',')
                    self.log.error(
                        "buildRobotiumConfig: browserEnv - Found a ',' in our value, unable to process value. key=%s,value=%s" %
                        (key, value))
                    self.log.error("browserEnv=%s" % browserEnv)
                except ValueError:
                    envstr += "%s%s=%s" % (delim, key, value)
                    delim = ","

            fHandle.write("envvars=%s\n" % envstr)
        fHandle.close()

        self._dm.removeFile(os.path.join(deviceRoot, "robotium.config"))
        self._dm.pushFile(
            fHandle.name,
            os.path.join(
                deviceRoot,
                "robotium.config"))
        os.unlink(fHandle.name)

    def getGMPPluginPath(self, options):
        # TODO: bug 1149374
        return None

    def buildBrowserEnv(self, options, debugger=False):
        browserEnv = Mochitest.buildBrowserEnv(
            self,
            options,
            debugger=debugger)
        # override nsprLogs to avoid processing in Mochitest base class
        self.nsprLogs = None
        browserEnv["NSPR_LOG_FILE"] = os.path.join(
            self.remoteNSPR,
            self.nsprLogName)
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


def run_test_harness(options):
    message_logger = MessageLogger(logger=None)
    process_args = {'messageLogger': message_logger}
    auto = RemoteAutomation(None, "fennec", processArgs=process_args)

    if options is None:
        raise ValueError("Invalid options specified, use --help for a list of valid options")

    dm = options.dm
    auto.setDeviceManager(dm)
    mochitest = MochiRemote(auto, dm, options)

    log = mochitest.log
    message_logger.logger = log
    mochitest.message_logger = message_logger

    productPieces = options.remoteProductName.split('.')
    if (productPieces is not None):
        auto.setProduct(productPieces[0])
    else:
        auto.setProduct(options.remoteProductName)
    auto.setAppName(options.remoteappname)

    logParent = os.path.dirname(options.remoteLogFile)
    dm.mkDir(logParent)
    auto.setRemoteLog(options.remoteLogFile)
    auto.setServerInfo(options.webServer, options.httpPort, options.sslPort)

    mochitest.printDeviceInfo()

    # Add Android version (SDK level) to mozinfo so that manifest entries
    # can be conditional on android_version.
    androidVersion = dm.shellCheckOutput(['getprop', 'ro.build.version.sdk'])
    log.info(
        "Android sdk version '%s'; will use this to filter manifests" %
        str(androidVersion))
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
        # turning buffering off as it's not used in robocop
        message_logger.buffering = False

        # sut may wait up to 300 s for a robocop am process before returning
        dm.default_timeout = 320
        mp = TestManifest(strict=False)
        # TODO: pull this in dynamically
        mp.read(options.robocopIni)

        filters = []
        if options.totalChunks:
            filters.append(
                chunk_by_slice(options.thisChunk, options.totalChunks))
        robocop_tests = mp.active_tests(exists=False, filters=filters, **mozinfo.info)

        options.extraPrefs.append('browser.search.suggest.enabled=true')
        options.extraPrefs.append('browser.search.suggest.prompted=true')
        options.extraPrefs.append('layout.css.devPixelsPerPx=1.0')
        options.extraPrefs.append('browser.chrome.dynamictoolbar=false')
        options.extraPrefs.append('browser.snippets.enabled=false')
        options.extraPrefs.append('browser.casting.enabled=true')

        if (options.dm_trans == 'adb' and options.robocopApk):
            dm._checkCmd(["install", "-r", options.robocopApk])

        retVal = None
        # Filtering tests
        active_tests = []
        for test in robocop_tests:
            if options.testPath and options.testPath != test['name']:
                continue

            if 'disabled' in test:
                log.info(
                    'TEST-INFO | skipping %s | %s' %
                    (test['name'], test['disabled']))
                continue

            active_tests.append(test)

        log.suite_start([t['name'] for t in active_tests])

        for test in active_tests:
            # When running in a loop, we need to create a fresh profile for
            # each cycle
            if mochitest.localProfile:
                options.profilePath = mochitest.localProfile
                os.system("rm -Rf %s" % options.profilePath)
                options.profilePath = None
                mochitest.localProfile = options.profilePath

            options.app = "am"
            options.browserArgs = [
                "instrument",
                "-w",
                "-e",
                "deviceroot",
                deviceRoot,
                "-e",
                "class"]
            options.browserArgs.append(
                "org.mozilla.gecko.tests.%s" %
                test['name'])
            options.browserArgs.append(
                "org.mozilla.roboexample.test/org.mozilla.gecko.FennecInstrumentationTestRunner")
            mochitest.nsprLogName = "nspr-%s.log" % test['name']

            # If the test is for checking the import from bookmarks then make
            # sure there is data to import
            if test['name'] == "testImportFromAndroid":

                # Get the OS so we can run the insert in the apropriate
                # database and following the correct table schema
                osInfo = dm.getInfo("os")
                devOS = " ".join(osInfo['os'])

                if ("pandaboard" in devOS):
                    delete = [
                        'execsu',
                        'sqlite3',
                        "/data/data/com.android.browser/databases/browser2.db \'delete from bookmarks where _id > 14;\'"]
                else:
                    delete = [
                        'execsu',
                        'sqlite3',
                        "/data/data/com.android.browser/databases/browser.db \'delete from bookmarks where _id > 14;\'"]
                if (options.dm_trans == "sut"):
                    dm._runCmds([{"cmd": " ".join(delete)}])

                # Insert the bookmarks
                log.info(
                    "Insert bookmarks in the default android browser database")
                for i in range(20):
                    if ("pandaboard" in devOS):
                        cmd = [
                            'execsu',
                            'sqlite3',
                            "/data/data/com.android.browser/databases/browser2.db 'insert or replace into bookmarks(_id,title,url,folder,parent,position) values (" +
                            str(
                                30 +
                                i) +
                            ",\"Bookmark" +
                            str(i) +
                            "\",\"http://www.bookmark" +
                            str(i) +
                            ".com\",0,1," +
                            str(
                                100 +
                                i) +
                            ");'"]
                    else:
                        cmd = [
                            'execsu',
                            'sqlite3',
                            "/data/data/com.android.browser/databases/browser.db 'insert into bookmarks(title,url,bookmark) values (\"Bookmark" +
                            str(i) +
                            "\",\"http://www.bookmark" +
                            str(i) +
                            ".com\",1);'"]
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
                # Ensure earlier failures aren't overwritten by success on this
                # run
                if retVal is None or retVal == 0:
                    retVal = result
            except:
                log.error(
                    "Automation Error: Exception caught while running tests")
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
                        cmd_del = [
                            'execsu',
                            'sqlite3',
                            "/data/data/com.android.browser/databases/browser2.db \'delete from bookmarks where _id > 14;\'"]
                    else:
                        cmd_del = [
                            'execsu',
                            'sqlite3',
                            "/data/data/com.android.browser/databases/browser.db \'delete from bookmarks where _id > 14;\'"]
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
        mochitest.nsprLogName = "nspr.log"
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

    message_logger.finish()

    return retVal


def main(args=sys.argv[1:]):
    parser = MochitestArgumentParser(app='android')
    options = parser.parse_args(args)

    return run_test_harness(options)


if __name__ == "__main__":
    sys.exit(main())

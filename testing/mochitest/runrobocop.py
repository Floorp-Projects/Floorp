# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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


class RobocopTestRunner(Mochitest):
    """
       A test harness for Robocop. Robocop tests are UI tests for Firefox for Android,
       based on the Robotium test framework. This harness leverages some functionality
       from mochitest, for convenience.
    """
    auto = None
    dm = None
    # Some robocop tests run for >60 seconds without generating any output.
    NO_OUTPUT_TIMEOUT = 180

    def __init__(self, automation, devmgr, options):
        """
           Simple one-time initialization.
        """
        Mochitest.__init__(self, options)

        self.auto = automation
        self.dm = devmgr
        self.dm.default_timeout = 320
        self.options = options
        self.options.logFile = "robocop.log"
        self.environment = self.auto.environment
        self.deviceRoot = self.dm.getDeviceRoot()
        self.remoteProfile = options.remoteTestRoot + "/profile"
        self.remoteProfileCopy = options.remoteTestRoot + "/profile-copy"
        self.auto.setRemoteProfile(self.remoteProfile)
        self.remoteConfigFile = os.path.join(
            self.deviceRoot, "robotium.config")
        self.remoteLog = options.remoteLogFile
        self.auto.setRemoteLog(self.remoteLog)
        self.remoteScreenshots = "/mnt/sdcard/Robotium-Screenshots"
        self.remoteNSPR = os.path.join(options.remoteTestRoot, "nspr")
        self.auto.setServerInfo(
            self.options.webServer, self.options.httpPort, self.options.sslPort)
        self.localLog = options.logFile
        self.localProfile = None
        productPieces = self.options.remoteProductName.split('.')
        if (productPieces is not None):
            self.auto.setProduct(productPieces[0])
        else:
            self.auto.setProduct(self.options.remoteProductName)
        self.auto.setAppName(self.options.remoteappname)
        self.certdbNew = True
        self.remoteCopyAvailable = True
        self.passed = 0
        self.failed = 0
        self.todo = 0

    def startup(self):
        """
           Second-stage initialization: One-time initialization which may require cleanup.
        """
        # Despite our efforts to clean up servers started by this script, in practice
        # we still see infrequent cases where a process is orphaned and interferes
        # with future tests, typically because the old server is keeping the port in use.
        # Try to avoid those failures by checking for and killing orphan servers before
        # trying to start new ones.
        self.killNamedOrphans('ssltunnel')
        self.killNamedOrphans('xpcshell')
        self.auto.deleteANRs()
        self.auto.deleteTombstones()
        self.dm.killProcess(self.options.app.split('/')[-1])
        self.dm.removeDir(self.remoteScreenshots)
        self.dm.removeDir(self.remoteNSPR)
        self.dm.mkDir(self.remoteNSPR)
        self.dm.mkDir(os.path.dirname(self.options.remoteLogFile))
        # Add Android version (SDK level) to mozinfo so that manifest entries
        # can be conditional on android_version.
        androidVersion = self.dm.shellCheckOutput(
            ['getprop', 'ro.build.version.sdk'])
        self.log.info(
            "Android sdk version '%s'; will use this to filter manifests" %
            str(androidVersion))
        mozinfo.info['android_version'] = androidVersion
        if (self.options.dm_trans == 'adb' and self.options.robocopApk):
            self.dm._checkCmd(["install", "-r", self.options.robocopApk])
            self.log.debug("Robocop APK %s installed" %
                           self.options.robocopApk)
        # Display remote diagnostics; if running in mach, keep output terse.
        if self.options.log_mach is None:
            self.printDeviceInfo()
        self.setupLocalPaths()
        self.buildProfile()
        # ignoreSSLTunnelExts is a workaround for bug 1109310
        self.startServers(
            self.options,
            debuggerInfo=None,
            ignoreSSLTunnelExts=True)
        self.log.debug("Servers started")

    def cleanup(self):
        """
           Cleanup at end of job run.
        """
        self.log.debug("Cleaning up...")
        self.stopServers()
        self.dm.killProcess(self.options.app.split('/')[-1])
        blobberUploadDir = os.environ.get('MOZ_UPLOAD_DIR', None)
        if blobberUploadDir:
            self.log.debug("Pulling any remote nspr logs and screenshots to %s." %
                           blobberUploadDir)
            self.dm.getDirectory(self.remoteNSPR, blobberUploadDir)
            self.dm.getDirectory(self.remoteScreenshots, blobberUploadDir)
        Mochitest.cleanup(self, self.options)
        if self.localProfile:
            os.system("rm -Rf %s" % self.localProfile)
        self.dm.removeDir(self.remoteProfile)
        self.dm.removeDir(self.remoteProfileCopy)
        self.dm.removeDir(self.remoteScreenshots)
        self.dm.removeDir(self.remoteNSPR)
        self.dm.removeFile(self.remoteConfigFile)
        if self.dm.fileExists(self.remoteLog):
            self.dm.removeFile(self.remoteLog)
        self.log.debug("Cleanup complete.")

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

    def setupLocalPaths(self):
        """
           Setup xrePath and utilityPath and verify xpcshell.

           This is similar to switchToLocalPaths in runtestsremote.py.
        """
        localAutomation = self.makeLocalAutomation()
        paths = [
            self.options.xrePath,
            localAutomation.DIST_BIN,
            self.auto._product,
            os.path.join('..', self.auto._product)
        ]
        self.options.xrePath = self.findPath(paths)
        if self.options.xrePath is None:
            self.log.error(
                "unable to find xulrunner path for %s, please specify with --xre-path" %
                os.name)
            sys.exit(1)
        self.log.debug("using xre path %s" % self.options.xrePath)
        xpcshell = "xpcshell"
        if (os.name == "nt"):
            xpcshell += ".exe"
        if self.options.utilityPath:
            paths = [self.options.utilityPath, self.options.xrePath]
        else:
            paths = [self.options.xrePath]
        self.options.utilityPath = self.findPath(paths, xpcshell)
        if self.options.utilityPath is None:
            self.log.error(
                "unable to find utility path for %s, please specify with --utility-path" %
                os.name)
            sys.exit(1)
        self.log.debug("using utility path %s" % self.options.utilityPath)
        xpcshell_path = os.path.join(self.options.utilityPath, xpcshell)
        if localAutomation.elf_arm(xpcshell_path):
            self.log.error('xpcshell at %s is an ARM binary; please use '
                           'the --utility-path argument to specify the path '
                           'to a desktop version.' % xpcshell_path)
            sys.exit(1)
        self.log.debug("xpcshell found at %s" % xpcshell_path)

    def buildProfile(self):
        """
           Build a profile locally, keep it locally for use by servers and
           push a copy to the remote profile-copy directory.

           This is similar to buildProfile in runtestsremote.py.
        """
        self.options.extraPrefs.append('browser.search.suggest.enabled=true')
        self.options.extraPrefs.append('browser.search.suggest.prompted=true')
        self.options.extraPrefs.append('layout.css.devPixelsPerPx=1.0')
        self.options.extraPrefs.append('browser.chrome.dynamictoolbar=false')
        self.options.extraPrefs.append('browser.snippets.enabled=false')
        self.options.extraPrefs.append('browser.casting.enabled=true')
        self.options.extraPrefs.append('extensions.autoupdate.enabled=false')
        manifest = Mochitest.buildProfile(self, self.options)
        self.localProfile = self.options.profilePath
        self.log.debug("Profile created at %s" % self.localProfile)
        # some files are not needed for robocop; save time by not pushing
        shutil.rmtree(os.path.join(self.localProfile, 'webapps'))
        desktop_extensions = ['mochikit@mozilla.org', 'worker-test@mozilla.org', 'workerbootstrap-test@mozilla.org']
        for ext in desktop_extensions:
            shutil.rmtree(os.path.join(self.localProfile, 'extensions', 'staged', ext))
        os.remove(os.path.join(self.localProfile, 'userChrome.css'))
        try:
            self.dm.pushDir(self.localProfile, self.remoteProfileCopy)
        except devicemanager.DMError:
            self.log.error(
                "Automation Error: Unable to copy profile to device.")
            raise

        return manifest

    def setupRemoteProfile(self):
        """
           Remove any remote profile and re-create it.
        """
        self.log.debug("Updating remote profile at %s" % self.remoteProfile)
        self.dm.removeDir(self.remoteProfile)
        if self.remoteCopyAvailable:
            try:
                self.dm.shellCheckOutput(
                    ['cp', '-r', self.remoteProfileCopy, self.remoteProfile],
                    root=True, timeout=60)
            except devicemanager.DMError:
                # For instance, cp is not available on some older versions of
                # Android.
                self.log.info(
                    "Unable to copy remote profile; falling back to push.")
                self.remoteCopyAvailable = False
        if not self.remoteCopyAvailable:
            self.dm.pushDir(self.localProfile, self.remoteProfile)

    def parseLocalLog(self):
        """
           Read and parse the local log file, noting any failures.
        """
        with open(self.localLog) as currentLog:
            data = currentLog.readlines()
        os.unlink(self.localLog)
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
                if 'status' in message:
                    if 'expected' in message:
                        self.failed += 1
                    elif message['status'] == 'PASS':
                        self.passed += 1
                    elif message['status'] == 'FAIL':
                        self.todo += 1
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

    def logTestSummary(self):
        """
           Print a summary of all tests run to stdout, for treeherder parsing 
           (logging via self.log does not work here).
        """
        print("0 INFO TEST-START | Shutdown")
        print("1 INFO Passed: %s" % (self.passed))
        print("2 INFO Failed: %s" % (self.failed))
        print("3 INFO Todo: %s" % (self.todo))
        print("4 INFO SimpleTest FINISHED")
        if self.failed > 0:
            return 1
        return 0

    def printDeviceInfo(self, printLogcat=False):
        """
           Log remote device information and logcat (if requested).

           This is similar to printDeviceInfo in runtestsremote.py
        """
        try:
            if printLogcat:
                logcat = self.dm.getLogcat(
                    filterOutRegexps=fennecLogcatFilters)
                self.log.info(
                    '\n' +
                    ''.join(logcat).decode(
                        'utf-8',
                        'replace'))
            self.log.info("Device info:")
            devinfo = self.dm.getInfo()
            for category in devinfo:
                if type(devinfo[category]) is list:
                    self.log.info("  %s:" % category)
                    for item in devinfo[category]:
                        self.log.info("     %s" % item)
                else:
                    self.log.info("  %s: %s" % (category, devinfo[category]))
            self.log.info("Test root: %s" % self.dm.deviceRoot)
        except devicemanager.DMError:
            self.log.warning("Error getting device information")

    def setupRobotiumConfig(self, browserEnv):
        """
           Create robotium.config and push it to the device.
        """
        fHandle = tempfile.NamedTemporaryFile(suffix='.config',
                                              prefix='robotium-',
                                              dir=os.getcwd(),
                                              delete=False)
        fHandle.write("profile=%s\n" % (self.remoteProfile))
        fHandle.write("logfile=%s\n" % (self.options.remoteLogFile))
        fHandle.write("host=http://mochi.test:8888/tests\n")
        fHandle.write(
            "rawhost=http://%s:%s/tests\n" %
            (self.options.remoteWebServer, self.options.httpPort))
        if browserEnv:
            envstr = ""
            delim = ""
            for key, value in browserEnv.items():
                try:
                    value.index(',')
                    self.log.error(
                        "setupRobotiumConfig: browserEnv - Found a ',' in our value, unable to process value. key=%s,value=%s" %
                        (key, value))
                    self.log.error("browserEnv=%s" % browserEnv)
                except ValueError:
                    envstr += "%s%s=%s" % (delim, key, value)
                    delim = ","
            fHandle.write("envvars=%s\n" % envstr)
        fHandle.close()
        self.dm.removeFile(self.remoteConfigFile)
        self.dm.pushFile(fHandle.name, self.remoteConfigFile)
        os.unlink(fHandle.name)

    def buildBrowserEnv(self):
        """
           Return an environment dictionary suitable for remote use.

           This is similar to buildBrowserEnv in runtestsremote.py.
        """
        browserEnv = self.environment(
            xrePath=None,
            debugger=None)
        # remove desktop environment not used on device
        if "MOZ_WIN_INHERIT_STD_HANDLES_PRE_VISTA" in browserEnv:
            del browserEnv["MOZ_WIN_INHERIT_STD_HANDLES_PRE_VISTA"]
        if "XPCOM_MEM_BLOAT_LOG" in browserEnv:
            del browserEnv["XPCOM_MEM_BLOAT_LOG"]
        browserEnv["NSPR_LOG_FILE"] = os.path.join(
            self.remoteNSPR,
            self.nsprLogName)
        return browserEnv

    def runSingleTest(self, test):
        """
           Run the specified test.
        """
        self.log.debug("Running test %s" % test['name'])
        self.nsprLogName = "nspr-%s.log" % test['name']
        browserEnv = self.buildBrowserEnv()
        self.setupRobotiumConfig(browserEnv)
        self.setupRemoteProfile()
        self.options.app = "am"
        if self.options.autorun:
            # This launches a test (using "am instrument") and instructs
            # Fennec to /quit/ the browser (using Robocop:Quit) and to
            # /finish/ all opened activities.
            browserArgs = [
                "instrument",
                "-w",
                "-e", "quit_and_finish", "1",
                "-e", "deviceroot", self.deviceRoot,
                "-e", "class",
                "org.mozilla.gecko.tests.%s" % test['name'].split('.java')[0],
                "org.mozilla.roboexample.test/org.mozilla.gecko.FennecInstrumentationTestRunner"]
        else:
            # This does not launch a test at all. It launches an activity
            # that starts Fennec and then waits indefinitely, since cat
            # never returns.
            browserArgs = ["start",
                           "-n", "org.mozilla.roboexample.test/org.mozilla.gecko.LaunchFennecWithConfigurationActivity",
                           "&&", "cat"]
            self.dm.default_timeout = sys.maxint  # Forever.
            self.log.info("")
            self.log.info("Serving mochi.test Robocop root at http://%s:%s/tests/robocop/" %
                         (self.options.remoteWebServer, self.options.httpPort))
            self.log.info("")
        result = -1
        log_result = -1
        try:
            self.dm.recordLogcat()
            result = self.auto.runApp(
                None, browserEnv, "am", self.localProfile, browserArgs, timeout=self.NO_OUTPUT_TIMEOUT)
            self.log.debug("runApp completes with status %d" % result)
            if result != 0:
                self.log.error("runApp() exited with code %s" % result)
            if self.dm.fileExists(self.remoteLog):
                self.dm.getFile(self.remoteLog, self.localLog)
                self.dm.removeFile(self.remoteLog)
                self.log.debug("Remote log %s retrieved to %s" %
                               (self.remoteLog, self.localLog))
            else:
                self.log.warning(
                    "Unable to retrieve log file (%s) from remote device" %
                    self.remoteLog)
            log_result = self.parseLocalLog()
            if result != 0 or log_result != 0:
                # Display remote diagnostics; if running in mach, keep output
                # terse.
                if self.options.log_mach is None:
                    self.printDeviceInfo(printLogcat=True)
        except:
            self.log.error(
                "Automation Error: Exception caught while running tests")
            traceback.print_exc()
            result = 1
        self.log.debug("Test %s completes with status %d (log status %d)" %
                       (test['name'], int(result), int(log_result)))
        return result

    def runTests(self):
        self.startup()
        if isinstance(self.options.manifestFile, TestManifest):
            mp = self.options.manifestFile
        else:
            mp = TestManifest(strict=False)
            mp.read(self.options.robocopIni)
        filters = []
        if self.options.totalChunks:
            filters.append(
                chunk_by_slice(self.options.thisChunk, self.options.totalChunks))
        robocop_tests = mp.active_tests(
            exists=False, filters=filters, **mozinfo.info)
        if not self.options.autorun:
            # Force a single loop iteration. The iteration will start Fennec and
            # the httpd server, but not actually run a test.
            self.options.test_paths = [robocop_tests[0]['name']]
        active_tests = []
        for test in robocop_tests:
            if self.options.test_paths and test['name'] not in self.options.test_paths:
                continue
            if 'disabled' in test:
                self.log.info('TEST-INFO | skipping %s | %s' %
                             (test['name'], test['disabled']))
                continue
            active_tests.append(test)
        self.log.suite_start([t['name'] for t in active_tests])
        worstTestResult = None
        for test in active_tests:
            result = self.runSingleTest(test)
            if worstTestResult is None or worstTestResult == 0:
                worstTestResult = result
        if worstTestResult is None:
            self.log.warning(
                "No tests run. Did you pass an invalid TEST_PATH?")
            worstTestResult = 1
        else:
            print "INFO | runtests.py | Test summary: start."
            logResult = self.logTestSummary()
            print "INFO | runtests.py | Test summary: end."
            if worstTestResult == 0:
                worstTestResult = logResult
        return worstTestResult


def run_test_harness(options):
    if options is None:
        raise ValueError(
            "Invalid options specified, use --help for a list of valid options")
    message_logger = MessageLogger(logger=None)
    process_args = {'messageLogger': message_logger}
    auto = RemoteAutomation(None, "fennec", processArgs=process_args)
    auto.setDeviceManager(options.dm)
    runResult = -1
    robocop = RobocopTestRunner(auto, options.dm, options)
    try:
        message_logger.logger = robocop.log
        message_logger.buffering = False
        robocop.message_logger = message_logger
        robocop.log.debug("options=%s" % vars(options))
        runResult = robocop.runTests()
    except KeyboardInterrupt:
        robocop.log.info("runrobocop.py | Received keyboard interrupt")
        runResult = -1
    except:
        traceback.print_exc()
        robocop.log.error(
            "runrobocop.py | Received unexpected exception while running tests")
        runResult = 1
    finally:
        try:
            robocop.cleanup()
        except devicemanager.DMError:
            # ignore device error while cleaning up
            pass
        message_logger.finish()
    return runResult


def main(args=sys.argv[1:]):
    parser = MochitestArgumentParser(app='android')
    options = parser.parse_args(args)
    return run_test_harness(options)

if __name__ == "__main__":
    sys.exit(main())

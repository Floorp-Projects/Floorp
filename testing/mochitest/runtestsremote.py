# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import traceback

sys.path.insert(
    0, os.path.abspath(
        os.path.realpath(
            os.path.dirname(__file__))))

from automation import Automation
from remoteautomation import RemoteAutomation, fennecLogcatFilters
from runtests import MochitestDesktop, MessageLogger
from mochitest_options import MochitestArgumentParser

import mozdevice
import mozinfo

SCRIPT_DIR = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))


# TODO inherit from MochitestBase instead
class MochiRemote(MochitestDesktop):
    _automation = None
    _dm = None
    localProfile = None
    logMessages = []

    def __init__(self, automation, devmgr, options):
        MochitestDesktop.__init__(self, options)

        self._automation = automation
        self._dm = devmgr
        self.environment = self._automation.environment
        self.remoteProfile = os.path.join(options.remoteTestRoot, "profile/")
        self.remoteModulesDir = os.path.join(options.remoteTestRoot, "modules/")
        self._automation.setRemoteProfile(self.remoteProfile)
        self.remoteLog = options.remoteLogFile
        self.localLog = options.logFile
        self._automation.deleteANRs()
        self._automation.deleteTombstones()
        self.certdbNew = True
        self.remoteMozLog = os.path.join(options.remoteTestRoot, "mozlog")
        self._dm.removeDir(self.remoteMozLog)
        self._dm.mkDir(self.remoteMozLog)
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
        blobberUploadDir = os.environ.get('MOZ_UPLOAD_DIR', None)
        if blobberUploadDir:
            self._dm.getDirectory(self.remoteMozLog, blobberUploadDir)
        MochitestDesktop.cleanup(self, options)

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
        MochitestDesktop.startServers(
            self,
            options,
            debuggerInfo,
            ignoreSSLTunnelExts=True)
        restoreRemotePaths()

    def buildProfile(self, options):
        restoreRemotePaths = self.switchToLocalPaths(options)
        if options.testingModulesDir:
            try:
                self._dm.pushDir(options.testingModulesDir, self.remoteModulesDir)
                self._dm.chmodDir(self.remoteModulesDir)
            except mozdevice.DMError:
                self.log.error(
                    "Automation Error: Unable to copy test modules to device.")
                raise
            savedTestingModulesDir = options.testingModulesDir
            options.testingModulesDir = self.remoteModulesDir
        else:
            savedTestingModulesDir = None
        manifest = MochitestDesktop.buildProfile(self, options)
        if savedTestingModulesDir:
            options.testingModulesDir = savedTestingModulesDir
        self.localProfile = options.profilePath

        restoreRemotePaths()
        options.profilePath = self.remoteProfile
        return manifest

    def addChromeToProfile(self, options):
        manifest = MochitestDesktop.addChromeToProfile(self, options)

        # Support Firefox (browser), B2G (shell), SeaMonkey (navigator), and Webapp
        # Runtime (webapp).
        if options.flavor == 'chrome':
            # append overlay to chrome.manifest
            chrome = "overlay chrome://browser/content/browser.xul chrome://mochikit/content/browser-test-overlay.xul"
            path = os.path.join(options.profilePath, 'extensions', 'staged',
                                'mochikit@mozilla.org', 'chrome.manifest')
            with open(path, "a") as f:
                f.write(chrome)
        return manifest

    def buildURLOptions(self, options, env):
        self.localLog = options.logFile
        options.logFile = self.remoteLog
        options.fileLevel = 'INFO'
        options.profilePath = self.localProfile
        env["MOZ_HIDE_RESULTS_TABLE"] = "1"
        retVal = MochitestDesktop.buildURLOptions(self, options, env)

        # we really need testConfig.js (for browser chrome)
        try:
            self._dm.pushDir(options.profilePath, self.remoteProfile)
            self._dm.chmodDir(self.remoteProfile)
        except mozdevice.DMError:
            self.log.error(
                "Automation Error: Unable to copy profile to device.")
            raise

        options.profilePath = self.remoteProfile
        options.logFile = self.localLog
        return retVal

    def getChromeTestDir(self, options):
        local = super(MochiRemote, self).getChromeTestDir(options)
        local = os.path.join(local, "chrome")
        remote = self.remoteChromeTestDir
        if options.flavor == 'chrome':
            self.log.info("pushing %s to %s on device..." % (local, remote))
            self._dm.pushDir(local, remote)
        return remote

    def getLogFilePath(self, logFile):
        return logFile

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
        except mozdevice.DMError:
            self.log.warning("Error getting device information")

    def getGMPPluginPath(self, options):
        # TODO: bug 1149374
        return None

    def buildBrowserEnv(self, options, debugger=False):
        browserEnv = MochitestDesktop.buildBrowserEnv(
            self,
            options,
            debugger=debugger)
        # remove desktop environment not used on device
        if "MOZ_WIN_INHERIT_STD_HANDLES_PRE_VISTA" in browserEnv:
            del browserEnv["MOZ_WIN_INHERIT_STD_HANDLES_PRE_VISTA"]
        if "XPCOM_MEM_BLOAT_LOG" in browserEnv:
            del browserEnv["XPCOM_MEM_BLOAT_LOG"]
        # override mozLogs to avoid processing in MochitestDesktop base class
        self.mozLogs = None
        browserEnv["MOZ_LOG_FILE"] = os.path.join(
            self.remoteMozLog,
            self.mozLogName)
        return browserEnv

    def runApp(self, *args, **kwargs):
        """front-end automation.py's `runApp` functionality until FennecRunner is written"""

        # automation.py/remoteautomation `runApp` takes the profile path,
        # whereas runtest.py's `runApp` takes a mozprofile object.
        if 'profileDir' not in kwargs and 'profile' in kwargs:
            kwargs['profileDir'] = kwargs.pop('profile').profile

        # remove args not supported by automation.py
        kwargs.pop('marionette_args', None)
        kwargs.pop('quiet', None)

        return self._automation.runApp(*args, **kwargs)


def run_test_harness(parser, options):
    parser.validate(options)

    message_logger = MessageLogger(logger=None)
    process_args = {'messageLogger': message_logger}
    auto = RemoteAutomation(None, "fennec", processArgs=process_args)

    if options is None:
        raise ValueError("Invalid options specified, use --help for a list of valid options")

    options.runByDir = False
    # roboextender is used by mochitest-chrome tests like test_java_addons.html,
    # but not by any plain mochitests
    if options.flavor != 'chrome':
        options.extensionsToExclude.append('roboextender@mozilla.org')

    dm = options.dm
    auto.setDeviceManager(dm)
    mochitest = MochiRemote(auto, dm, options)

    log = mochitest.log
    message_logger.logger = log
    mochitest.message_logger = message_logger

    # Check that Firefox is installed
    expected = options.app.split('/')[-1]
    installed = dm.shellCheckOutput(['pm', 'list', 'packages', expected])
    if expected not in installed:
        log.error("%s is not installed on this device" % expected)
        return 1

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

    if options.log_mach is None:
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

    mochitest.mozLogName = "moz.log"
    try:
        dm.recordLogcat()
        retVal = mochitest.runTests(options)
    except:
        log.error("Automation Error: Exception caught while running tests")
        traceback.print_exc()
        mochitest.stopServers()
        try:
            mochitest.cleanup(options)
        except mozdevice.DMError:
            # device error cleaning up... oh well!
            pass
        retVal = 1

    if options.log_mach is None:
        mochitest.printDeviceInfo(printLogcat=True)

    message_logger.finish()

    return retVal


def main(args=sys.argv[1:]):
    parser = MochitestArgumentParser(app='android')
    options = parser.parse_args(args)

    return run_test_harness(parser, options)


if __name__ == "__main__":
    sys.exit(main())

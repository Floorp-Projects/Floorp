# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import posixpath
import sys
import traceback
import uuid

sys.path.insert(
    0, os.path.abspath(
        os.path.realpath(
            os.path.dirname(__file__))))

from remoteautomation import RemoteAutomation, fennecLogcatFilters
from runtests import MochitestDesktop, MessageLogger
from mochitest_options import MochitestArgumentParser, build_obj
from mozdevice import ADBDevice, ADBTimeoutError
from mozscreenshot import dump_screen, dump_device_screen
import mozinfo

SCRIPT_DIR = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))


class MochiRemote(MochitestDesktop):
    localProfile = None
    logMessages = []

    def __init__(self, options):
        MochitestDesktop.__init__(self, options.flavor, vars(options))

        verbose = False
        if options.log_tbpl_level == 'debug' or options.log_mach_level == 'debug':
            verbose = True
        if hasattr(options, 'log'):
            delattr(options, 'log')

        self.certdbNew = True
        self.chromePushed = False

        self.device = ADBDevice(adb=options.adbPath or 'adb',
                                device=options.deviceSerial,
                                test_root=options.remoteTestRoot,
                                verbose=verbose)

        if options.remoteTestRoot is None:
            options.remoteTestRoot = self.device.test_root
        options.dumpOutputDirectory = options.remoteTestRoot
        self.remoteLogFile = posixpath.join(options.remoteTestRoot, "logs", "mochitest.log")
        logParent = posixpath.dirname(self.remoteLogFile)
        self.device.rm(logParent, force=True, recursive=True)
        self.device.mkdir(logParent)

        self.remoteProfile = posixpath.join(options.remoteTestRoot, "profile/")
        self.device.rm(self.remoteProfile, force=True, recursive=True, root=True)

        self.counts = dict()
        self.message_logger = MessageLogger(logger=None)
        self.message_logger.logger = self.log
        process_args = {'messageLogger': self.message_logger, 'counts': self.counts}
        self.automation = RemoteAutomation(self.device, options.remoteappname, self.remoteProfile,
                                           self.remoteLogFile, processArgs=process_args)
        self.environment = self.automation.environment

        # Check that Firefox is installed
        expected = options.app.split('/')[-1]
        if not self.device.is_app_installed(expected):
            raise Exception("%s is not installed on this device" % expected)

        self.device.clear_logcat()

        self.remoteModulesDir = posixpath.join(options.remoteTestRoot, "modules/")

        self.remoteCache = posixpath.join(options.remoteTestRoot, "cache/")
        self.device.rm(self.remoteCache, force=True, recursive=True, root=True)

        # move necko cache to a location that can be cleaned up
        options.extraPrefs += ["browser.cache.disk.parent_directory=%s" % self.remoteCache]

        self.remoteMozLog = posixpath.join(options.remoteTestRoot, "mozlog")
        self.device.rm(self.remoteMozLog, force=True, recursive=True, root=True)
        self.device.mkdir(self.remoteMozLog)

        self.remoteChromeTestDir = posixpath.join(
            options.remoteTestRoot,
            "chrome")
        self.device.rm(self.remoteChromeTestDir, force=True, recursive=True, root=True)
        self.device.mkdir(self.remoteChromeTestDir)

        procName = options.app.split('/')[-1]
        self.device.stop_application(procName)
        if self.device.process_exist(procName):
            self.log.warning("unable to kill %s before running tests!" % procName)

        # Add Android version (SDK level) to mozinfo so that manifest entries
        # can be conditional on android_version.
        self.log.info(
            "Android sdk version '%s'; will use this to filter manifests" %
            str(self.device.version))
        mozinfo.info['android_version'] = str(self.device.version)
        mozinfo.info['is_fennec'] = not ('geckoview' in options.app)
        mozinfo.info['is_emulator'] = self.device._device_serial.startswith('emulator-')

    def cleanup(self, options, final=False):
        if final:
            self.device.rm(self.remoteChromeTestDir, force=True, recursive=True, root=True)
            self.chromePushed = False
            uploadDir = os.environ.get('MOZ_UPLOAD_DIR', None)
            if uploadDir and self.device.is_dir(self.remoteMozLog):
                self.device.pull(self.remoteMozLog, uploadDir)
        self.device.rm(self.remoteLogFile, force=True, root=True)
        self.device.rm(self.remoteProfile, force=True, recursive=True, root=True)
        self.device.rm(self.remoteCache, force=True, recursive=True, root=True)
        MochitestDesktop.cleanup(self, options, final)
        self.localProfile = None

    def dumpScreen(self, utilityPath):
        if self.haveDumpedScreen:
            self.log.info(
                "Not taking screenshot here: see the one that was previously logged")
            return
        self.haveDumpedScreen = True
        if self.device._device_serial.startswith('emulator-'):
            dump_screen(utilityPath, self.log)
        else:
            dump_device_screen(self.device, self.log)

    def findPath(self, paths, filename=None):
        for path in paths:
            p = path
            if filename:
                p = os.path.join(p, filename)
            if os.path.exists(self.getFullPath(p)):
                return path
        return None

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

        paths = [
            options.xrePath,
        ]
        if build_obj:
            paths.append(os.path.join(build_obj.topobjdir, "dist", "bin"))
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
        if RemoteAutomation.elf_arm(xpcshell_path):
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

    def startServers(self, options, debuggerInfo, public=None):
        """ Create the servers on the host and start them up """
        restoreRemotePaths = self.switchToLocalPaths(options)
        MochitestDesktop.startServers(
            self,
            options,
            debuggerInfo,
            public=True)
        restoreRemotePaths()

    def buildProfile(self, options):
        restoreRemotePaths = self.switchToLocalPaths(options)
        if options.testingModulesDir:
            try:
                self.device.push(options.testingModulesDir, self.remoteModulesDir)
                self.device.chmod(self.remoteModulesDir, recursive=True, root=True)
            except Exception:
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

    def buildURLOptions(self, options, env):
        saveLogFile = options.logFile
        options.logFile = self.remoteLogFile
        options.profilePath = self.localProfile
        env["MOZ_HIDE_RESULTS_TABLE"] = "1"
        retVal = MochitestDesktop.buildURLOptions(self, options, env)

        # we really need testConfig.js (for browser chrome)
        try:
            self.device.push(options.profilePath, self.remoteProfile)
            self.device.chmod(self.remoteProfile, recursive=True, root=True)
        except Exception:
            self.log.error("Automation Error: Unable to copy profile to device.")
            raise

        options.profilePath = self.remoteProfile
        options.logFile = saveLogFile
        return retVal

    def getChromeTestDir(self, options):
        local = super(MochiRemote, self).getChromeTestDir(options)
        remote = self.remoteChromeTestDir
        if options.flavor == 'chrome' and not self.chromePushed:
            self.log.info("pushing %s to %s on device..." % (local, remote))
            local = os.path.join(local, "chrome")
            self.device.push(local, remote)
            self.chromePushed = True
        return remote

    def getLogFilePath(self, logFile):
        return logFile

    def printDeviceInfo(self, printLogcat=False):
        try:
            if printLogcat:
                logcat = self.device.get_logcat(
                    filter_out_regexps=fennecLogcatFilters)
                for l in logcat:
                    ul = l.decode('utf-8', errors='replace')
                    sl = ul.encode('iso8859-1', errors='replace')
                    self.log.info(sl)
            self.log.info("Device info:")
            devinfo = self.device.get_info()
            for category in devinfo:
                if type(devinfo[category]) is list:
                    self.log.info("  %s:" % category)
                    for item in devinfo[category]:
                        self.log.info("     %s" % item)
                else:
                    self.log.info("  %s: %s" % (category, devinfo[category]))
            self.log.info("Test root: %s" % self.device.test_root)
        except ADBTimeoutError:
            raise
        except Exception as e:
            self.log.warning("Error getting device information: %s" % str(e))

    def getGMPPluginPath(self, options):
        # TODO: bug 1149374
        return None

    def buildBrowserEnv(self, options, debugger=False):
        browserEnv = MochitestDesktop.buildBrowserEnv(
            self,
            options,
            debugger=debugger)
        # remove desktop environment not used on device
        if "XPCOM_MEM_BLOAT_LOG" in browserEnv:
            del browserEnv["XPCOM_MEM_BLOAT_LOG"]
        if self.mozLogs:
            browserEnv["MOZ_LOG_FILE"] = os.path.join(
                self.remoteMozLog,
                "moz-pid=%PID-uid={}.log".format(str(uuid.uuid4())))
        if options.dmd:
            browserEnv['DMD'] = '1'
        # Contents of remoteMozLog will be pulled from device and copied to the
        # host MOZ_UPLOAD_DIR, to be made available as test artifacts. Make
        # MOZ_UPLOAD_DIR available to the browser environment so that tests
        # can use it as though they were running on the host.
        browserEnv["MOZ_UPLOAD_DIR"] = self.remoteMozLog
        return browserEnv

    def runApp(self, *args, **kwargs):
        """front-end automation's `runApp` functionality until FennecRunner is written"""

        # remoteautomation `runApp` takes the profile path,
        # whereas runtest.py's `runApp` takes a mozprofile object.
        if 'profileDir' not in kwargs and 'profile' in kwargs:
            kwargs['profileDir'] = kwargs.pop('profile').profile

        # remove args not supported by automation
        kwargs.pop('marionette_args', None)

        ret, _ = self.automation.runApp(*args, **kwargs)
        self.countpass += self.counts['pass']
        self.countfail += self.counts['fail']
        self.counttodo += self.counts['todo']

        return ret, None


def run_test_harness(parser, options):
    parser.validate(options)

    if options is None:
        raise ValueError("Invalid options specified, use --help for a list of valid options")

    options.runByManifest = True
    # roboextender is used by mochitest-chrome tests like test_java_addons.html,
    # but not by any plain mochitests
    if options.flavor != 'chrome':
        options.extensionsToExclude.append('roboextender@mozilla.org')

    mochitest = MochiRemote(options)

    if options.log_mach is None and not options.verify:
        mochitest.printDeviceInfo()

    try:
        device_exception = False
        if options.verify:
            retVal = mochitest.verifyTests(options)
        else:
            retVal = mochitest.runTests(options)
    except Exception as e:
        mochitest.log.error("Automation Error: Exception caught while running tests")
        traceback.print_exc()
        if isinstance(e, ADBTimeoutError):
            mochitest.log.info("Device disconnected. Will not run mochitest.cleanup().")
            device_exception = True
        else:
            try:
                mochitest.cleanup(options)
            except Exception:
                # device error cleaning up... oh well!
                traceback.print_exc()
        retVal = 1

    if not device_exception and options.log_mach is None and not options.verify:
        mochitest.printDeviceInfo(printLogcat=(retVal != 0))

    mochitest.archiveMozLogs()
    mochitest.message_logger.finish()

    return retVal


def main(args=sys.argv[1:]):
    parser = MochitestArgumentParser(app='android')
    options = parser.parse_args(args)

    return run_test_harness(parser, options)


if __name__ == "__main__":
    sys.exit(main())

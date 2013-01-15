# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import ConfigParser
import os
import sys
import tempfile
import traceback

sys.path.insert(0, os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0]))))

from automation import Automation
from b2gautomation import B2GRemoteAutomation
from runtests import Mochitest
from runtests import MochitestOptions
from runtests import MochitestServer

import devicemanager
import devicemanagerADB

from marionette import Marionette


class B2GOptions(MochitestOptions):

    def __init__(self, automation, scriptdir, **kwargs):
        defaults = {}
        MochitestOptions.__init__(self, automation, scriptdir)

        self.add_option("--b2gpath", action="store",
                    type = "string", dest = "b2gPath",
                    help = "path to B2G repo or qemu dir")
        defaults["b2gPath"] = None

        self.add_option("--marionette", action="store",
                    type = "string", dest = "marionette",
                    help = "host:port to use when connecting to Marionette")
        defaults["marionette"] = None

        self.add_option("--emulator", action="store",
                    type="string", dest = "emulator",
                    help = "Architecture of emulator to use: x86 or arm")
        defaults["emulator"] = None

        self.add_option("--sdcard", action="store", 
                    type="string", dest = "sdcard", 
                    help = "Define size of sdcard: 1MB, 50MB...etc")
        defaults["sdcard"] = None

        self.add_option("--no-window", action="store_true",
                    dest = "noWindow",
                    help = "Pass --no-window to the emulator")
        defaults["noWindow"] = False

        self.add_option("--adbpath", action="store",
                    type = "string", dest = "adbPath",
                    help = "path to adb")
        defaults["adbPath"] = "adb"

        self.add_option("--deviceIP", action="store",
                    type = "string", dest = "deviceIP",
                    help = "ip address of remote device to test")
        defaults["deviceIP"] = None

        self.add_option("--devicePort", action="store",
                    type = "string", dest = "devicePort",
                    help = "port of remote device to test")
        defaults["devicePort"] = 20701

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

        self.add_option("--pidfile", action = "store",
                    type = "string", dest = "pidFile",
                    help = "name of the pidfile to generate")
        defaults["pidFile"] = ""

        self.add_option("--gecko-path", action="store",
                        type="string", dest="geckoPath",
                        help="the path to a gecko distribution that should "
                        "be installed on the emulator prior to test")
        defaults["geckoPath"] = None
        self.add_option("--logcat-dir", action="store",
                        type="string", dest="logcat_dir",
                        help="directory to store logcat dump files")
        defaults["logcat_dir"] = None
        self.add_option('--busybox', action='store',
                        type='string', dest='busybox',
                        help="Path to busybox binary to install on device")
        defaults['busybox'] = None

        defaults["remoteTestRoot"] = "/data/local/tests"
        defaults["logFile"] = "mochitest.log"
        defaults["autorun"] = True
        defaults["closeWhenDone"] = True
        defaults["testPath"] = ""
        defaults["extensionsToExclude"] = ["specialpowers"]

        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options, automation):
        if not options.remoteTestRoot:
            options.remoteTestRoot = automation._devicemanager.getDeviceRoot()
        productRoot = options.remoteTestRoot + "/" + automation._product

        if options.utilityPath == self._automation.DIST_BIN:
            options.utilityPath = productRoot + "/bin"

        if options.remoteWebServer == None:
            if os.name != "nt":
                options.remoteWebServer = automation.getLanIp()
            else:
                self.error("You must specify a --remote-webserver=<ip address>")
        options.webServer = options.remoteWebServer

        if options.geckoPath and not options.emulator:
            self.error("You must specify --emulator if you specify --gecko-path")

        if options.logcat_dir and not options.emulator:
            self.error("You must specify --emulator if you specify --logcat-dir")

        #if not options.emulator and not options.deviceIP:
        #    print "ERROR: you must provide a device IP"
        #    return None

        if options.remoteLogFile == None:
            options.remoteLogFile = options.remoteTestRoot + '/logs/mochitest.log'

        if options.remoteLogFile.count('/') < 1:
            options.remoteLogFile = options.remoteTestRoot + '/' + options.remoteLogFile

        # Only reset the xrePath if it wasn't provided
        if options.xrePath == None:
            options.xrePath = options.utilityPath

        if options.pidFile != "":
            f = open(options.pidFile, 'w')
            f.write("%s" % os.getpid())
            f.close()

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


class ProfileConfigParser(ConfigParser.RawConfigParser):
    """Subclass of RawConfigParser that outputs .ini files in the exact
       format expected for profiles.ini, which is slightly different
       than the default format.
    """

    def optionxform(self, optionstr):
        return optionstr

    def write(self, fp):
        if self._defaults:
            fp.write("[%s]\n" % ConfigParser.DEFAULTSECT)
            for (key, value) in self._defaults.items():
                fp.write("%s=%s\n" % (key, str(value).replace('\n', '\n\t')))
            fp.write("\n")
        for section in self._sections:
            fp.write("[%s]\n" % section)
            for (key, value) in self._sections[section].items():
                if key == "__name__":
                    continue
                if (value is not None) or (self._optcre == self.OPTCRE):
                    key = "=".join((key, str(value).replace('\n', '\n\t')))
                fp.write("%s\n" % (key))
            fp.write("\n")


class B2GMochitest(Mochitest):

    _automation = None
    _dm = None
    localProfile = None

    def __init__(self, automation, devmgr, options):
        self._automation = automation
        Mochitest.__init__(self, self._automation)
        self._dm = devmgr
        self.runSSLTunnel = False
        self.remoteProfile = options.remoteTestRoot + '/profile'
        self._automation.setRemoteProfile(self.remoteProfile)
        self.remoteLog = options.remoteLogFile
        self.localLog = None
        self.userJS = '/data/local/user.js'
        self.remoteMozillaPath = '/data/b2g/mozilla'
        self.bundlesDir = '/system/b2g/distribution/bundles'
        self.remoteProfilesIniPath = os.path.join(self.remoteMozillaPath, 'profiles.ini')
        self.originalProfilesIni = None

    def copyRemoteFile(self, src, dest):
        if self._dm._useDDCopy:
            self._dm._checkCmdAs(['shell', 'dd', 'if=%s' % src,'of=%s' % dest])
        else:
            self._dm._checkCmdAs(['shell', 'cp', src, dest])

    def origUserJSExists(self):
        return self._dm.fileExists('/data/local/user.js.orig')

    def cleanup(self, manifest, options):
        if self.localLog:
            self._dm.getFile(self.remoteLog, self.localLog)
            self._dm.removeFile(self.remoteLog)

        # Delete any bundled extensions
        extensionDir = os.path.join(options.profilePath, 'extensions', 'staged')
        if os.access(extensionDir, os.F_OK):
            for filename in os.listdir(extensionDir):
                try:
                    self._dm._checkCmdAs(['shell', 'rm', '-rf',
                                          os.path.join(self.bundlesDir, filename)])
                except devicemanager.DMError:
                    pass

        if not options.emulator:
            # Remove the test profile
            self._dm._checkCmdAs(['shell', 'rm', '-r', self.remoteProfile])

            if self.origUserJSExists():
                # Restore the original user.js
                self._dm.removeFile(self.userJS)
                self.copyRemoteFile('%s.orig' % self.userJS, self.userJS)
                self._dm.removeFile("%s.orig" % self.userJS)

            if self._dm.fileExists('%s.orig' % self.remoteProfilesIniPath):
                # Restore the original profiles.ini
                self._dm.removeFile(self.remoteProfilesIniPath)
                self.copyRemoteFile('%s.orig' % self.remoteProfilesIniPath,
                                    self.remoteProfilesIniPath)
                self._dm.removeFile("%s.orig" % self.remoteProfilesIniPath)

            # We've restored the original profile, so reboot the device so that
            # it gets picked up.
            self._automation.rebootDevice()

        if options.pidFile != "":
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
        if hostos in ['mac', 'darwin']:
            localAutomation.IS_MAC = True
        elif hostos in ['linux', 'linux2']:
            localAutomation.IS_LINUX = True
            localAutomation.UNIXISH = True
        elif hostos in ['win32', 'win64']:
            localAutomation.BIN_SUFFIX = ".exe"
            localAutomation.IS_WIN32 = True

        paths = [options.xrePath,
                 localAutomation.DIST_BIN,
                 self._automation._product,
                 os.path.join('..', self._automation._product)]
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
        if hasattr(self, 'server'):
            self.server.stop()

    def buildProfile(self, options):
        if self.localProfile:
            options.profilePath = self.localProfile
        manifest = Mochitest.buildProfile(self, options)
        self.localProfile = options.profilePath

        # Profile isn't actually copied to device until
        # buildURLOptions is called.

        options.profilePath = self.remoteProfile
        return manifest

    def updateProfilesIni(self, profilePath):
        # update profiles.ini on the device to point to the test profile
        self.originalProfilesIni = tempfile.mktemp()
        self._dm.getFile(self.remoteProfilesIniPath, self.originalProfilesIni)

        config = ProfileConfigParser()
        config.read(self.originalProfilesIni)
        for section in config.sections():
            if 'Profile' in section:
                config.set(section, 'IsRelative', 0)
                config.set(section, 'Path', profilePath)

        newProfilesIni = tempfile.mktemp()
        with open(newProfilesIni, 'wb') as configfile:
            config.write(configfile)

        self._dm.pushFile(newProfilesIni, self.remoteProfilesIniPath)
        self._dm.pushFile(self.originalProfilesIni, '%s.orig' % self.remoteProfilesIniPath)

        try:
            os.remove(newProfilesIni)
            os.remove(self.originalProfilesIni)
        except:
            pass

    def buildURLOptions(self, options, env):
        self.localLog = options.logFile
        options.logFile = self.remoteLog
        options.profilePath = self.localProfile
        retVal = Mochitest.buildURLOptions(self, options, env)

        # set the testURL
        testURL = self.buildTestPath(options)
        if len(self.urlOpts) > 0:
            testURL += "?" + "&".join(self.urlOpts)
        self._automation.testURL = testURL

        # execute this script on start up.
        # loads special powers and sets the test-container
        # apps's iframe to the mochitest URL.
        self._automation.test_script = """
const CHILD_SCRIPT = "chrome://specialpowers/content/specialpowers.js";
const CHILD_SCRIPT_API = "chrome://specialpowers/content/specialpowersAPI.js";
const CHILD_LOGGER_SCRIPT = "chrome://specialpowers/content/MozillaLogger.js";

let homescreen = document.getElementById('homescreen');
let container = homescreen.contentWindow.document.getElementById('test-container');
container.setAttribute('mozapp', 'http://mochi.test:8888/manifest.webapp');

let specialpowers = {};
let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader);
loader.loadSubScript("chrome://specialpowers/content/SpecialPowersObserver.js", specialpowers);
let specialPowersObserver = new specialpowers.SpecialPowersObserver();
specialPowersObserver.init();

let mm = container.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
mm.addMessageListener("SPPrefService", specialPowersObserver);
mm.addMessageListener("SPProcessCrashService", specialPowersObserver);
mm.addMessageListener("SPPingService", specialPowersObserver);
mm.addMessageListener("SpecialPowers.Quit", specialPowersObserver);
mm.addMessageListener("SPPermissionManager", specialPowersObserver);

mm.loadFrameScript(CHILD_LOGGER_SCRIPT, true);
mm.loadFrameScript(CHILD_SCRIPT_API, true);
mm.loadFrameScript(CHILD_SCRIPT, true);
specialPowersObserver._isFrameScriptLoaded = true;

container.src = '%s';
""" % testURL

        # Set extra prefs for B2G.
        f = open(os.path.join(options.profilePath, "user.js"), "a")
        f.write("""
user_pref("browser.homescreenURL","app://test-container.gaiamobile.org/index.html");
user_pref("browser.manifestURL","app://test-container.gaiamobile.org/manifest.webapp");
user_pref("dom.mozBrowserFramesEnabled", true);
user_pref("dom.ipc.tabs.disabled", false);
user_pref("dom.ipc.browser_frames.oop_by_default", false);
user_pref("dom.mozBrowserFramesWhitelist","app://test-container.gaiamobile.org,http://mochi.test:8888");
user_pref("marionette.loadearly", true);
""")
        f.close()

        # Copy the profile to the device.
        self._dm._checkCmdAs(['shell', 'rm', '-r', self.remoteProfile])
        try:
            self._dm.pushDir(options.profilePath, self.remoteProfile)
        except devicemanager.DMError:
            print "Automation Error: Unable to copy profile to device."
            raise

        # Copy the extensions to the B2G bundles dir.
        extensionDir = os.path.join(options.profilePath, 'extensions', 'staged')
        # need to write to read-only dir
        self._dm._checkCmdAs(['remount'])
        for filename in os.listdir(extensionDir):
            self._dm._checkCmdAs(['shell', 'rm', '-rf',
                                  os.path.join(self.bundlesDir, filename)])
        try:
            self._dm.pushDir(extensionDir, self.bundlesDir)
        except devicemanager.DMError:
            print "Automation Error: Unable to copy extensions to device."
            raise

        # In B2G, user.js is always read from /data/local, not the profile
        # directory.  Backup the original user.js first so we can restore it.
        if not self._dm.fileExists('%s.orig' % self.userJS):
            self.copyRemoteFile(self.userJS, '%s.orig' % self.userJS)
        self._dm.pushFile(os.path.join(options.profilePath, "user.js"), self.userJS)
        self.updateProfilesIni(self.remoteProfile)
        options.profilePath = self.remoteProfile
        options.logFile = self.localLog
        return retVal


def main():
    scriptdir = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))
    auto = B2GRemoteAutomation(None, "fennec")
    parser = B2GOptions(auto, scriptdir)
    options, args = parser.parse_args()

    # create our Marionette instance
    kwargs = {}
    if options.emulator:
        kwargs['emulator'] = options.emulator
        auto.setEmulator(True)
        if options.noWindow:
            kwargs['noWindow'] = True
        if options.geckoPath:
            kwargs['gecko_path'] = options.geckoPath
        if options.logcat_dir:
            kwargs['logcat_dir'] = options.logcat_dir
        if options.busybox:
            kwargs['busybox'] = options.busybox
    # needless to say sdcard is only valid if using an emulator
    if options.sdcard:
        kwargs['sdcard'] = options.sdcard
    if options.b2gPath:
        kwargs['homedir'] = options.b2gPath
    if options.marionette:
        host,port = options.marionette.split(':')
        kwargs['host'] = host
        kwargs['port'] = int(port)

    marionette = Marionette.getMarionetteOrExit(**kwargs)

    auto.marionette = marionette

    # create the DeviceManager
    kwargs = {'adbPath': options.adbPath,
              'deviceRoot': options.remoteTestRoot}
    if options.deviceIP:
        kwargs.update({'host': options.deviceIP,
                       'port': options.devicePort})
    dm = devicemanagerADB.DeviceManagerADB(**kwargs)
    auto.setDeviceManager(dm)
    options = parser.verifyRemoteOptions(options, auto)
    if (options == None):
        print "ERROR: Invalid options specified, use --help for a list of valid options"
        sys.exit(1)

    auto.setProduct("b2g")

    mochitest = B2GMochitest(auto, dm, options)

    options = parser.verifyOptions(options, mochitest)
    if (options == None):
        sys.exit(1)

    logParent = os.path.dirname(options.remoteLogFile)
    dm.mkDir(logParent)
    auto.setRemoteLog(options.remoteLogFile)
    auto.setServerInfo(options.webServer, options.httpPort, options.sslPort)
    retVal = 1
    try:
        mochitest.cleanup(None, options)
        retVal = mochitest.runTests(options)
    except:
        print "Automation Error: Exception caught while running tests"
        traceback.print_exc()
        mochitest.stopWebServer(options)
        mochitest.stopWebSocketServer(options)
        try:
            mochitest.cleanup(None, options)
        except:
            pass
        retVal = 1

    sys.exit(retVal)

if __name__ == "__main__":
    main()


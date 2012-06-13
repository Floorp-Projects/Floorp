# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import ConfigParser
import os
import re
import sys
import tempfile
import time
import urllib

sys.path.insert(0, os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0]))))

from automation import Automation
from b2gautomation import B2GRemoteAutomation
from runtests import Mochitest
from runtests import MochitestOptions
from runtests import MochitestServer

import devicemanagerADB
import manifestparser

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

        defaults["remoteTestRoot"] = None
        defaults["logFile"] = "mochitest.log"
        defaults["autorun"] = True
        defaults["closeWhenDone"] = True
        defaults["testPath"] = ""

        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options, automation):
        options.remoteTestRoot = automation._devicemanager.getDeviceRoot()
        productRoot = options.remoteTestRoot + "/" + automation._product

        if options.utilityPath == self._automation.DIST_BIN:
            options.utilityPath = productRoot + "/bin"

        if options.remoteWebServer == None:
            if os.name != "nt":
                options.remoteWebServer = automation.getLanIp()
            else:
                print "ERROR: you must specify a --remote-webserver=<ip address>\n"
                return None

        options.webServer = options.remoteWebServer

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
        self.userJS = '/data/local/user.js'
        self.testDir = '/data/local/tests'
        self.remoteMozillaPath = '/data/b2g/mozilla'
        self.remoteProfilesIniPath = os.path.join(self.remoteMozillaPath, 'profiles.ini')
        self.originalProfilesIni = None

    def cleanup(self, manifest, options):
        # Restore the original profiles.ini.
        if self.originalProfilesIni:
            try:
                if not options.emulator:
                    self.restoreProfilesIni()
                os.remove(self.originalProfilesIni)
            except:
                pass

        if not options.emulator:
            self._dm.getFile(self.remoteLog, self.localLog)
            self._dm.removeFile(self.remoteLog)
            self._dm.removeDir(self.remoteProfile)

            # Restore the original user.js.
            self._dm.checkCmdAs(['shell', 'rm', '-f', self.userJS])
            if self._dm.useDDCopy:
                self._dm.checkCmdAs(['shell', 'dd', 'if=%s.orig' % self.userJS, 'of=%s' % self.userJS])
            else:
                self._dm.checkCmdAs(['shell', 'cp', '%s.orig' % self.userJS, self.userJS])

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

    def restoreProfilesIni(self):
        # restore profiles.ini on the device to its previous state
        if not self.originalProfilesIni or not os.access(self.originalProfilesIni, os.F_OK):
            raise DMError('Unable to install original profiles.ini; file not found: %s',
                          self.originalProfilesIni)

        self._dm.pushFile(self.originalProfilesIni, self.remoteProfilesIniPath)

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
        try:
            os.remove(newProfilesIni)
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

        # Set the B2G homepage as a static local page, since wi-fi generally
        # isn't available as soon as the device boots.
        f = open(os.path.join(options.profilePath, "user.js"), "a")
        f.write('user_pref("browser.homescreenURL", "data:text/html,<h1>mochitest-plain should start soon</h1>");\n')
        f.close()

        # Copy the profile to the device.
        self._dm.removeDir(self.remoteProfile)
        if self._dm.pushDir(options.profilePath, self.remoteProfile) == None:
            raise devicemanager.FileError("Unable to copy profile to device.")

        # In B2G, user.js is always read from /data/local, not the profile
        # directory.  Backup the original user.js first so we can restore it.
        self._dm.checkCmdAs(['shell', 'rm', '-f', '%s.orig' % self.userJS])
        if self._dm.useDDCopy:
            self._dm.checkCmdAs(['shell', 'dd', 'if=%s' % self.userJS, 'of=%s.orig' % self.userJS])
        else:
            self._dm.checkCmdAs(['shell', 'cp', self.userJS, '%s.orig' % self.userJS])
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
    if options.b2gPath:
        kwargs['homedir'] = options.b2gPath
    if options.marionette:
        host,port = options.marionette.split(':')
        kwargs['host'] = host
        kwargs['port'] = int(port)
    marionette = Marionette(**kwargs)

    auto.marionette = marionette

    # create the DeviceManager
    kwargs = {'adbPath': options.adbPath}
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

    # Create /data/local/tests, to force its use by DeviceManagerADB;
    # B2G won't run correctly with the profile installed to /mnt/sdcard.
    dm.mkDirs(mochitest.testDir)

    options = parser.verifyOptions(options, mochitest)
    if (options == None):
        sys.exit(1)

    logParent = os.path.dirname(options.remoteLogFile)
    dm.mkDir(logParent);
    auto.setRemoteLog(options.remoteLogFile)
    auto.setServerInfo(options.webServer, options.httpPort, options.sslPort)

    retVal = 1
    try:
        retVal = mochitest.runTests(options)
    except:
        print "TEST-UNEXPECTED-FAIL | %s | Exception caught while running tests." % sys.exc_info()[1]
        mochitest.stopWebServer(options)
        mochitest.stopWebSocketServer(options)
        try:
            mochitest.cleanup(None, options)
        except:
            pass
            sys.exit(1)

    sys.exit(retVal)

if __name__ == "__main__":
    main()


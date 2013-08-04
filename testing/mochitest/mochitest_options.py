# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import optparse
import os
import sys
import tempfile

from automation import Automation
from automationutils import addCommonOptions, isURL
from mozprofile import DEFAULT_PORTS
import moznetwork

try:
    from mozbuild.base import MozbuildObject
    build_obj = MozbuildObject.from_environment()
except ImportError:
    build_obj = None

here = os.path.abspath(os.path.dirname(sys.argv[0]))

__all__ = ["MochitestOptions", "B2GOptions"]

VMWARE_RECORDING_HELPER_BASENAME = "vmwarerecordinghelper"

class MochitestOptions(optparse.OptionParser):
    """Usage instructions for runtests.py.
    All arguments are optional.
    If --chrome is specified, chrome tests will be run instead of web content tests.
    If --browser-chrome is specified, browser-chrome tests will be run instead of web content tests.
    See <http://mochikit.com/doc/html/MochiKit/Logging.html> for details on the logging levels.
    """

    LOG_LEVELS = ("DEBUG", "INFO", "WARNING", "ERROR", "FATAL")
    LEVEL_STRING = ", ".join(LOG_LEVELS)
    mochitest_options = [
        [["--close-when-done"],
        { "action": "store_true",
          "dest": "closeWhenDone",
          "default": False,
          "help": "close the application when tests are done running",
        }],
        [["--appname"],
        { "action": "store",
          "type": "string",
          "dest": "app",
          "default": None,
          "help": "absolute path to application, overriding default",
        }],
        [["--utility-path"],
        { "action": "store",
          "type": "string",
          "dest": "utilityPath",
          "default": build_obj.bindir if build_obj is not None else None,
          "help": "absolute path to directory containing utility programs (xpcshell, ssltunnel, certutil)",
        }],
        [["--certificate-path"],
        { "action": "store",
          "type": "string",
          "dest": "certPath",
          "help": "absolute path to directory containing certificate store to use testing profile",
          "default": os.path.join(build_obj.topsrcdir, 'build', 'pgo', 'certs') if build_obj is not None else None,
        }],
        [["--log-file"],
        { "action": "store",
          "type": "string",
          "dest": "logFile",
          "metavar": "FILE",
          "help": "file to which logging occurs",
          "default": "",
        }],
        [["--autorun"],
        { "action": "store_true",
          "dest": "autorun",
          "help": "start running tests when the application starts",
          "default": False,
        }],
        [["--timeout"],
        { "type": "int",
          "dest": "timeout",
          "help": "per-test timeout in seconds",
          "default": None,
        }],
        [["--total-chunks"],
        { "type": "int",
          "dest": "totalChunks",
          "help": "how many chunks to split the tests up into",
          "default": None,
        }],
        [["--this-chunk"],
        { "type": "int",
          "dest": "thisChunk",
          "help": "which chunk to run",
          "default": None,
        }],
        [["--chunk-by-dir"],
        { "type": "int",
          "dest": "chunkByDir",
          "help": "group tests together in the same chunk that are in the same top chunkByDir directories",
          "default": 0,
        }],
        [["--shuffle"],
        { "dest": "shuffle",
          "action": "store_true",
          "help": "randomize test order",
          "default": False,
        }],
        [["--console-level"],
        { "action": "store",
          "type": "choice",
          "dest": "consoleLevel",
          "choices": LOG_LEVELS,
          "metavar": "LEVEL",
          "help": "one of %s to determine the level of console "
                  "logging" % LEVEL_STRING,
          "default": None,
        }],
        [["--file-level"],
        { "action": "store",
          "type": "choice",
          "dest": "fileLevel",
          "choices": LOG_LEVELS,
          "metavar": "LEVEL",
          "help": "one of %s to determine the level of file "
                 "logging if a file has been specified, defaulting "
                 "to INFO" % LEVEL_STRING,
          "default": "INFO",
        }],
        [["--chrome"],
        { "action": "store_true",
          "dest": "chrome",
          "help": "run chrome Mochitests",
          "default": False,
        }],
        [["--ipcplugins"],
        { "action": "store_true",
          "dest": "ipcplugins",
          "help": "run ipcplugins Mochitests",
          "default": False,
        }],
        [["--test-path"],
        { "action": "store",
          "type": "string",
          "dest": "testPath",
          "help": "start in the given directory's tests",
          "default": "",
        }],
        [["--browser-chrome"],
        { "action": "store_true",
          "dest": "browserChrome",
          "help": "run browser chrome Mochitests",
          "default": False,
        }],
        [["--webapprt-content"],
        { "action": "store_true",
          "dest": "webapprtContent",
          "help": "run WebappRT content tests",
          "default": False,
        }],
        [["--webapprt-chrome"],
        { "action": "store_true",
          "dest": "webapprtChrome",
          "help": "run WebappRT chrome tests",
          "default": False,
        }],
        [["--a11y"],
        { "action": "store_true",
          "dest": "a11y",
          "help": "run accessibility Mochitests",
          "default": False,
        }],
        [["--setenv"],
        { "action": "append",
          "type": "string",
          "dest": "environment",
          "metavar": "NAME=VALUE",
          "help": "sets the given variable in the application's "
                 "environment",
          "default": [],
        }],
        [["--exclude-extension"],
        { "action": "append",
          "type": "string",
          "dest": "extensionsToExclude",
          "help": "excludes the given extension from being installed "
                 "in the test profile",
          "default": [],
        }],
        [["--browser-arg"],
        { "action": "append",
          "type": "string",
          "dest": "browserArgs",
          "metavar": "ARG",
          "help": "provides an argument to the test application",
          "default": [],
        }],
        [["--leak-threshold"],
        { "action": "store",
          "type": "int",
          "dest": "leakThreshold",
          "metavar": "THRESHOLD",
          "help": "fail if the number of bytes leaked through "
                 "refcounted objects (or bytes in classes with "
                 "MOZ_COUNT_CTOR and MOZ_COUNT_DTOR) is greater "
                 "than the given number",
          "default": 0,
        }],
        [["--fatal-assertions"],
        { "action": "store_true",
          "dest": "fatalAssertions",
          "help": "abort testing whenever an assertion is hit "
                 "(requires a debug build to be effective)",
          "default": False,
        }],
        [["--extra-profile-file"],
        { "action": "append",
          "dest": "extraProfileFiles",
          "help": "copy specified files/dirs to testing profile",
          "default": [],
        }],
        [["--install-extension"],
        { "action": "append",
          "dest": "extensionsToInstall",
          "help": "install the specified extension in the testing profile."
                 "The extension file's name should be <id>.xpi where <id> is"
                 "the extension's id as indicated in its install.rdf."
                 "An optional path can be specified too.",
          "default": [],
        }],
        [["--profile-path"],
        { "action": "store",
          "type": "string",
          "dest": "profilePath",
          "help": "Directory where the profile will be stored."
                 "This directory will be deleted after the tests are finished",
          "default": tempfile.mkdtemp(),
        }],
        [["--testing-modules-dir"],
        { "action": "store",
          "type": "string",
          "dest": "testingModulesDir",
          "help": "Directory where testing-only JS modules are located.",
          "default": None,
        }],
        [["--use-vmware-recording"],
        { "action": "store_true",
          "dest": "vmwareRecording",
          "help": "enables recording while the application is running "
                 "inside a VMware Workstation 7.0 or later VM",
          "default": False,
        }],
        [["--repeat"],
        { "action": "store",
          "type": "int",
          "dest": "repeat",
          "metavar": "REPEAT",
          "help": "repeats the test or set of tests the given number of times, ie: repeat: 1 will run the test twice.",
          "default": 0,
        }],
        [["--run-until-failure"],
        { "action": "store_true",
          "dest": "runUntilFailure",
          "help": "Run a test repeatedly and stops on the first time the test fails. "
                "Only available when running a single test. Default cap is 30 runs, "
                "which can be overwritten with the --repeat parameter.",
          "default": False,
        }],
        [["--run-only-tests"],
        { "action": "store",
          "type": "string",
          "dest": "runOnlyTests",
          "help": "JSON list of tests that we only want to run. [DEPRECATED- please use --test-manifest]",
          "default": None,
        }],
        [["--test-manifest"],
        { "action": "store",
          "type": "string",
          "dest": "testManifest",
          "help": "JSON list of tests to specify 'runtests'. Old format for mobile specific tests",
          "default": None,
        }],
        [["--manifest"],
        { "action": "store",
          "type": "string",
          "dest": "manifestFile",
          "help": ".ini format of tests to run.",
          "default": None,
        }],
        [["--failure-file"],
        { "action": "store",
          "type": "string",
          "dest": "failureFile",
          "help": "Filename of the output file where we can store a .json list of failures to be run in the future with --run-only-tests.",
          "default": None,
        }],
        [["--run-slower"],
        { "action": "store_true",
          "dest": "runSlower",
          "help": "Delay execution between test files.",
          "default": False,
        }],
        [["--metro-immersive"],
        { "action": "store_true",
          "dest": "immersiveMode",
          "help": "launches tests in immersive browser",
          "default": False,
        }],
        [["--httpd-path"],
        { "action": "store",
          "type": "string",
          "dest": "httpdPath",
          "default": None,
          "help": "path to the httpd.js file",
        }],
        [["--setpref"],
        { "action": "append",
          "type": "string",
          "default": [],
          "dest": "extraPrefs",
          "metavar": "PREF=VALUE",
          "help": "defines an extra user preference",
        }],
        [["--build-info-json"],
        { "action": "store",
          "type": "string",
          "default": None,
          "dest": "mozInfo",
          "help": "path to mozinfo.json to determine build time options",
        }],
    ]

    def __init__(self, automation=None, **kwargs):
        self._automation = automation or Automation()
        optparse.OptionParser.__init__(self, **kwargs)
        defaults = {}

        # we want to pass down everything from self._automation.__all__
        addCommonOptions(self, defaults=dict(zip(self._automation.__all__,
                 [getattr(self._automation, x) for x in self._automation.__all__])))

        for option in self.mochitest_options:
            self.add_option(*option[0], **option[1])

        self.set_defaults(**defaults)
        self.set_usage(self.__doc__)

    def verifyOptions(self, options, mochitest):
        """ verify correct options and cleanup paths """

        if options.app is None:
            if build_obj is not None:
                options.app = build_obj.get_binary_path()
            else:
                self.error("could not find the application path, --appname must be specified")

        if options.totalChunks is not None and options.thisChunk is None:
            self.error("thisChunk must be specified when totalChunks is specified")

        if options.totalChunks:
            if not 1 <= options.thisChunk <= options.totalChunks:
                self.error("thisChunk must be between 1 and totalChunks")

        if options.xrePath is None:
            # default xrePath to the app path if not provided
            # but only if an app path was explicitly provided
            if options.app != self.defaults['app']:
                options.xrePath = os.path.dirname(options.app)
            elif build_obj is not None:
                # otherwise default to dist/bin
                options.xrePath = build_obj.bindir
            else:
                self.error("could not find xre directory, --xre-path must be specified")

        # allow relative paths
        options.xrePath = mochitest.getFullPath(options.xrePath)
        options.profilePath = mochitest.getFullPath(options.profilePath)
        options.app = mochitest.getFullPath(options.app)

        if not os.path.exists(options.app):
            msg = """\
            Error: Path %(app)s doesn't exist.
            Are you executing $objdir/_tests/testing/mochitest/runtests.py?"""
            self.error(msg % {"app": options.app})
            return None

        if options.utilityPath:
            options.utilityPath = mochitest.getFullPath(options.utilityPath)

        if options.certPath:
            options.certPath = mochitest.getFullPath(options.certPath)

        if options.symbolsPath and not isURL(options.symbolsPath):
            options.symbolsPath = mochitest.getFullPath(options.symbolsPath)

        options.webServer = self._automation.DEFAULT_WEB_SERVER
        options.httpPort = self._automation.DEFAULT_HTTP_PORT
        options.sslPort = self._automation.DEFAULT_SSL_PORT
        options.webSocketPort = self._automation.DEFAULT_WEBSOCKET_PORT

        if options.vmwareRecording:
            if not self._automation.IS_WIN32:
                self.error("use-vmware-recording is only supported on Windows.")
            mochitest.vmwareHelperPath = os.path.join(
                options.utilityPath, VMWARE_RECORDING_HELPER_BASENAME + ".dll")
            if not os.path.exists(mochitest.vmwareHelperPath):
                self.error("%s not found, cannot automate VMware recording." %
                           mochitest.vmwareHelperPath)

        if options.testManifest and options.runOnlyTests:
            self.error("Please use --test-manifest only and not --run-only-tests")

        if options.runOnlyTests:
            if not os.path.exists(os.path.abspath(options.runOnlyTests)):
                self.error("unable to find --run-only-tests file '%s'" % options.runOnlyTests)
            options.runOnly = True
            options.testManifest = options.runOnlyTests
            options.runOnlyTests = None

        if options.manifestFile and options.testManifest:
            self.error("Unable to support both --manifest and --test-manifest/--run-only-tests at the same time")

        if options.webapprtContent and options.webapprtChrome:
            self.error("Only one of --webapprt-content and --webapprt-chrome may be given.")

        # Try to guess the testing modules directory.
        # This somewhat grotesque hack allows the buildbot machines to find the
        # modules directory without having to configure the buildbot hosts. This
        # code should never be executed in local runs because the build system
        # should always set the flag that populates this variable. If buildbot ever
        # passes this argument, this code can be deleted.
        if options.testingModulesDir is None:
            possible = os.path.join(os.getcwd(), os.path.pardir, 'modules')

            if os.path.isdir(possible):
                options.testingModulesDir = possible

        # Even if buildbot is updated, we still want this, as the path we pass in
        # to the app must be absolute and have proper slashes.
        if options.testingModulesDir is not None:
            options.testingModulesDir = os.path.normpath(options.testingModulesDir)

            if not os.path.isabs(options.testingModulesDir):
                options.testingModulesDir = os.path.abspath(options.testingModulesDir)

            if not os.path.isdir(options.testingModulesDir):
                self.error('--testing-modules-dir not a directory: %s' %
                    options.testingModulesDir)

            options.testingModulesDir = options.testingModulesDir.replace('\\', '/')
            if options.testingModulesDir[-1] != '/':
                options.testingModulesDir += '/'

        if options.immersiveMode:
            if not self._automation.IS_WIN32:
                self.error("immersive is only supported on Windows 8 and up.")
            mochitest.immersiveHelperPath = os.path.join(
                options.utilityPath, "metrotestharness.exe")
            if not os.path.exists(mochitest.immersiveHelperPath):
                self.error("%s not found, cannot launch immersive tests." %
                           mochitest.immersiveHelperPath)

        if options.runUntilFailure:
            if not os.path.isfile(os.path.join(mochitest.oldcwd, os.path.dirname(__file__), mochitest.getTestRoot(options), options.testPath)):
                self.error("--run-until-failure can only be used together with --test-path specifying a single test.")
            if not options.repeat:
                options.repeat = 29

        if not options.mozInfo:
            if build_obj:
                options.mozInfo = os.path.join(build_obj.get_binary_path(), 'mozinfo.json')
            else:
                options.mozInfo = os.path.abspath('mozinfo.json')

        if not os.path.isfile(options.mozInfo):
            self.error("Unable to file build information file (mozinfo.json) at this location: %s" % options.mozInfo)

        return options


class B2GOptions(MochitestOptions):
    b2g_options = [
        [["--b2gpath"],
        { "action": "store",
          "type": "string",
          "dest": "b2gPath",
          "help": "path to B2G repo or qemu dir",
          "default": None,
        }],
        [["--desktop"],
        { "action": "store_true",
          "dest": "desktop",
          "help": "Run the tests on a B2G desktop build",
          "default": False,
        }],
        [["--marionette"],
        { "action": "store",
          "type": "string",
          "dest": "marionette",
          "help": "host:port to use when connecting to Marionette",
          "default": None,
        }],
        [["--emulator"],
        { "action": "store",
          "type": "string",
          "dest": "emulator",
          "help": "Architecture of emulator to use: x86 or arm",
          "default": None,
        }],
        [["--sdcard"],
        { "action": "store",
          "type": "string",
          "dest": "sdcard",
          "help": "Define size of sdcard: 1MB, 50MB...etc",
          "default": "10MB",
        }],
        [["--no-window"],
        { "action": "store_true",
          "dest": "noWindow",
          "help": "Pass --no-window to the emulator",
          "default": False,
        }],
        [["--adbpath"],
        { "action": "store",
          "type": "string",
          "dest": "adbPath",
          "help": "path to adb",
          "default": "adb",
        }],
        [["--deviceIP"],
        { "action": "store",
          "type": "string",
          "dest": "deviceIP",
          "help": "ip address of remote device to test",
          "default": None,
        }],
        [["--devicePort"],
        { "action": "store",
          "type": "string",
          "dest": "devicePort",
          "help": "port of remote device to test",
          "default": 20701,
        }],
        [["--remote-logfile"],
        { "action": "store",
          "type": "string",
          "dest": "remoteLogFile",
          "help": "Name of log file on the device relative to the device root. \
                  PLEASE ONLY USE A FILENAME.",
          "default" : None,
        }],
        [["--remote-webserver"],
        { "action": "store",
          "type": "string",
          "dest": "remoteWebServer",
          "help": "ip address where the remote web server is hosted at",
          "default": None,
        }],
        [["--http-port"],
        { "action": "store",
          "type": "string",
          "dest": "httpPort",
          "help": "ip address where the remote web server is hosted at",
          "default": None,
        }],
        [["--ssl-port"],
        { "action": "store",
          "type": "string",
          "dest": "sslPort",
          "help": "ip address where the remote web server is hosted at",
          "default": None,
        }],
        [["--pidfile"],
        { "action": "store",
          "type": "string",
          "dest": "pidFile",
          "help": "name of the pidfile to generate",
          "default": "",
        }],
        [["--gecko-path"],
        { "action": "store",
          "type": "string",
          "dest": "geckoPath",
          "help": "the path to a gecko distribution that should \
                   be installed on the emulator prior to test",
          "default": None,
        }],
        [["--profile"],
        { "action": "store",
          "type": "string",
          "dest": "profile",
          "help": "for desktop testing, the path to the \
                   gaia profile to use",
          "default": None,
        }],
        [["--logcat-dir"],
        { "action": "store",
          "type": "string",
          "dest": "logcat_dir",
          "help": "directory to store logcat dump files",
          "default": None,
        }],
        [['--busybox'],
        { "action": 'store',
          "type": 'string',
          "dest": 'busybox',
          "help": "Path to busybox binary to install on device",
          "default": None,
        }],
        [['--profile-data-dir'],
        { "action": 'store',
          "type": 'string',
          "dest": 'profile_data_dir',
          "help": "Path to a directory containing preference and other \
                   data to be installed into the profile",
          "default": os.path.join(here, 'profile_data'),
        }],
    ]

    def __init__(self):
        MochitestOptions.__init__(self)

        for option in self.b2g_options:
            self.add_option(*option[0], **option[1])

        defaults = {}
        defaults["httpPort"] = DEFAULT_PORTS['http']
        defaults["sslPort"] = DEFAULT_PORTS['https']
        defaults["remoteTestRoot"] = "/data/local/tests"
        defaults["logFile"] = "mochitest.log"
        defaults["autorun"] = True
        defaults["closeWhenDone"] = True
        defaults["testPath"] = ""
        defaults["extensionsToExclude"] = ["specialpowers"]
        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options):
        if options.remoteWebServer == None:
            if os.name != "nt":
                options.remoteWebServer = moznetwork.get_ip()
            else:
                self.error("You must specify a --remote-webserver=<ip address>")
        options.webServer = options.remoteWebServer

        if options.geckoPath and not options.emulator:
            self.error("You must specify --emulator if you specify --gecko-path")

        if options.logcat_dir and not options.emulator:
            self.error("You must specify --emulator if you specify --logcat-dir")

        if not os.path.isdir(options.xrePath):
            self.error("--xre-path '%s' is not a directory" % options.xrePath)
        xpcshell = os.path.join(options.xrePath, 'xpcshell')
        if not os.access(xpcshell, os.F_OK):
            self.error('xpcshell not found at %s' % xpcshell)
        if self.elf_arm(xpcshell):
            self.error('--xre-path points to an ARM version of xpcshell; it '
                       'should instead point to a version that can run on '
                       'your desktop')

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

    def elf_arm(self, filename):
        data = open(filename, 'rb').read(20)
        return data[:4] == "\x7fELF" and ord(data[18]) == 40 # EM_ARM

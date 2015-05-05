# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from argparse import ArgumentParser
from urlparse import urlparse
import logging
import os
import tempfile

from mozprofile import DEFAULT_PORTS
import mozinfo
import moznetwork

from automation import Automation

here = os.path.abspath(os.path.dirname(__file__))

try:
    from mozbuild.base import MozbuildObject
    build_obj = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build_obj = None

__all__ = ["MochitestOptions", "B2GOptions"]

VMWARE_RECORDING_HELPER_BASENAME = "vmwarerecordinghelper"


class MochitestOptions(ArgumentParser):
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
         {"action": "store_true",
          "dest": "closeWhenDone",
          "default": False,
          "help": "close the application when tests are done running",
          }],
        [["--appname"],
         {"dest": "app",
          "default": None,
          "help": "absolute path to application, overriding default",
          }],
        [["--utility-path"],
         {"dest": "utilityPath",
          "default": build_obj.bindir if build_obj is not None else None,
          "help": "absolute path to directory containing utility programs (xpcshell, ssltunnel, certutil)",
          }],
        [["--certificate-path"],
         {"dest": "certPath",
          "help": "absolute path to directory containing certificate store to use testing profile",
          "default": os.path.join(build_obj.topsrcdir, 'build', 'pgo', 'certs') if build_obj is not None else None,
          }],
        [["--autorun"],
         {"action": "store_true",
          "dest": "autorun",
          "help": "start running tests when the application starts",
          "default": False,
          }],
        [["--timeout"],
         {"type": int,
          "dest": "timeout",
          "help": "per-test timeout in seconds",
          "default": None,
          }],
        [["--total-chunks"],
         {"type": int,
          "dest": "totalChunks",
          "help": "how many chunks to split the tests up into",
          "default": None,
          }],
        [["--this-chunk"],
         {"type": int,
          "dest": "thisChunk",
          "help": "which chunk to run",
          "default": None,
          }],
        [["--chunk-by-runtime"],
         {"action": "store_true",
          "dest": "chunkByRuntime",
          "help": "group tests such that each chunk has roughly the same runtime",
          "default": False,
          }],
        [["--chunk-by-dir"],
         {"type": int,
          "dest": "chunkByDir",
          "help": "group tests together in the same chunk that are in the same top chunkByDir directories",
          "default": 0,
          }],
        [["--run-by-dir"],
         {"action": "store_true",
          "dest": "runByDir",
          "help": "Run each directory in a single browser instance with a fresh profile",
          "default": False,
          }],
        [["--shuffle"],
         {"dest": "shuffle",
          "action": "store_true",
          "help": "randomize test order",
          "default": False,
          }],
        [["--console-level"],
         {"dest": "consoleLevel",
          "choices": LOG_LEVELS,
          "metavar": "LEVEL",
          "help": "one of %s to determine the level of console "
                  "logging" % LEVEL_STRING,
          "default": None,
          }],
        [["--chrome"],
         {"action": "store_true",
          "dest": "chrome",
          "help": "run chrome Mochitests",
          "default": False,
          }],
        [["--ipcplugins"],
         {"action": "store_true",
          "dest": "ipcplugins",
          "help": "run ipcplugins Mochitests",
          "default": False,
          }],
        [["--test-path"],
         {"dest": "testPath",
          "help": "start in the given directory's tests",
          "default": "",
          }],
        [["--bisect-chunk"],
         {"dest": "bisectChunk",
          "help": "Specify the failing test name to find the previous tests that may be causing the failure.",
          "default": None,
          }],
        [["--start-at"],
         {"dest": "startAt",
          "help": "skip over tests until reaching the given test",
          "default": "",
          }],
        [["--end-at"],
         {"dest": "endAt",
          "help": "don't run any tests after the given one",
          "default": "",
          }],
        [["--browser-chrome"],
         {"action": "store_true",
          "dest": "browserChrome",
          "help": "run browser chrome Mochitests",
          "default": False,
          }],
        [["--subsuite"],
         {"dest": "subsuite",
          "help": "subsuite of tests to run",
          "default": None,
          }],
        [["--jetpack-package"],
         {"action": "store_true",
          "dest": "jetpackPackage",
          "help": "run jetpack package tests",
          "default": False,
          }],
        [["--jetpack-addon"],
         {"action": "store_true",
          "dest": "jetpackAddon",
          "help": "run jetpack addon tests",
          "default": False,
          }],
        [["--webapprt-content"],
         {"action": "store_true",
          "dest": "webapprtContent",
          "help": "run WebappRT content tests",
          "default": False,
          }],
        [["--webapprt-chrome"],
         {"action": "store_true",
          "dest": "webapprtChrome",
          "help": "run WebappRT chrome tests",
          "default": False,
          }],
        [["--a11y"],
         {"action": "store_true",
          "dest": "a11y",
          "help": "run accessibility Mochitests",
          "default": False,
          }],
        [["--setenv"],
         {"action": "append",
          "dest": "environment",
          "metavar": "NAME=VALUE",
          "help": "sets the given variable in the application's "
          "environment",
          "default": [],
          }],
        [["--exclude-extension"],
         {"action": "append",
          "dest": "extensionsToExclude",
          "help": "excludes the given extension from being installed "
          "in the test profile",
          "default": [],
          }],
        [["--browser-arg"],
         {"action": "append",
          "dest": "browserArgs",
          "metavar": "ARG",
          "help": "provides an argument to the test application",
          "default": [],
          }],
        [["--leak-threshold"],
         {"type": int,
          "dest": "defaultLeakThreshold",
          "metavar": "THRESHOLD",
          "help": "fail if the number of bytes leaked in default "
          "processes through refcounted objects (or bytes "
          "in classes with MOZ_COUNT_CTOR and MOZ_COUNT_DTOR) "
          "is greater than the given number",
          "default": 0,
          }],
        [["--fatal-assertions"],
         {"action": "store_true",
          "dest": "fatalAssertions",
          "help": "abort testing whenever an assertion is hit "
          "(requires a debug build to be effective)",
          "default": False,
          }],
        [["--extra-profile-file"],
         {"action": "append",
          "dest": "extraProfileFiles",
          "help": "copy specified files/dirs to testing profile",
          "default": [],
          }],
        [["--install-extension"],
         {"action": "append",
          "dest": "extensionsToInstall",
          "help": "install the specified extension in the testing profile."
          "The extension file's name should be <id>.xpi where <id> is"
          "the extension's id as indicated in its install.rdf."
          "An optional path can be specified too.",
          "default": [],
          }],
        [["--profile-path"],
         {"dest": "profilePath",
          "help": "Directory where the profile will be stored."
          "This directory will be deleted after the tests are finished",
          "default": None,
          }],
        [["--testing-modules-dir"],
         {"dest": "testingModulesDir",
          "help": "Directory where testing-only JS modules are located.",
          "default": None,
          }],
        [["--use-vmware-recording"],
         {"action": "store_true",
          "dest": "vmwareRecording",
          "help": "enables recording while the application is running "
          "inside a VMware Workstation 7.0 or later VM",
          "default": False,
          }],
        [["--repeat"],
         {"type": int,
          "dest": "repeat",
          "metavar": "REPEAT",
          "help": "repeats the test or set of tests the given number of times, ie: repeat: 1 will run the test twice.",
          "default": 0,
          }],
        [["--run-until-failure"],
         {"action": "store_true",
          "dest": "runUntilFailure",
          "help": "Run tests repeatedly and stops on the first time a test fails. "
          "Default cap is 30 runs, which can be overwritten with the --repeat parameter.",
          "default": False,
          }],
        [["--manifest"],
         {"dest": "manifestFile",
          "help": ".ini format of tests to run.",
          "default": None,
          }],
        [["--testrun-manifest-file"],
         {"dest": "testRunManifestFile",
          "help": "Overrides the default filename of the tests.json manifest file that is created from the manifest and used by the test runners to run the tests. Only useful when running multiple test runs simulatenously on the same machine.",
          "default": 'tests.json',
          }],
        [["--failure-file"],
         {"dest": "failureFile",
          "help": "Filename of the output file where we can store a .json list of failures to be run in the future with --run-only-tests.",
          "default": None,
          }],
        [["--run-slower"],
         {"action": "store_true",
          "dest": "runSlower",
          "help": "Delay execution between test files.",
          "default": False,
          }],
        [["--metro-immersive"],
         {"action": "store_true",
          "dest": "immersiveMode",
          "help": "launches tests in immersive browser",
          "default": False,
          }],
        [["--httpd-path"],
         {"dest": "httpdPath",
          "default": None,
          "help": "path to the httpd.js file",
          }],
        [["--setpref"],
         {"action": "append",
          "default": [],
          "dest": "extraPrefs",
          "metavar": "PREF=VALUE",
          "help": "defines an extra user preference",
          }],
        [["--jsdebugger"],
         {"action": "store_true",
          "default": False,
          "dest": "jsdebugger",
          "help": "open the browser debugger",
          }],
        [["--debug-on-failure"],
         {"action": "store_true",
          "default": False,
          "dest": "debugOnFailure",
          "help": "breaks execution and enters the JS debugger on a test failure. Should be used together with --jsdebugger."
          }],
        [["--e10s"],
         {"action": "store_true",
          "default": False,
          "dest": "e10s",
          "help": "Run tests with electrolysis preferences and test filtering enabled.",
          }],
        [["--strict-content-sandbox"],
         {"action": "store_true",
          "default": False,
          "dest": "strictContentSandbox",
          "help": "Run tests with a more strict content sandbox (Windows only).",
          }],
        [["--nested_oop"],
         {"action": "store_true",
          "default": False,
          "dest": "nested_oop",
          "help": "Run tests with nested_oop preferences and test filtering enabled.",
          }],
        [["--dmd-path"],
         {"default": None,
          "dest": "dmdPath",
          "help": "Specifies the path to the directory containing the shared library for DMD.",
          }],
        [["--dump-output-directory"],
         {"default": None,
          "dest": "dumpOutputDirectory",
          "help": "Specifies the directory in which to place dumped memory reports.",
          }],
        [["--dump-about-memory-after-test"],
         {"action": "store_true",
          "default": False,
          "dest": "dumpAboutMemoryAfterTest",
          "help": "Produce an about:memory dump after each test in the directory specified "
          "by --dump-output-directory."
          }],
        [["--dump-dmd-after-test"],
         {"action": "store_true",
          "default": False,
          "dest": "dumpDMDAfterTest",
          "help": "Produce a DMD dump after each test in the directory specified "
          "by --dump-output-directory."
          }],
        [["--slowscript"],
         {"action": "store_true",
          "default": False,
          "dest": "slowscript",
          "help": "Do not set the JS_DISABLE_SLOW_SCRIPT_SIGNALS env variable; "
          "when not set, recoverable but misleading SIGSEGV instances "
          "may occur in Ion/Odin JIT code."
          }],
        [["--screenshot-on-fail"],
         {"action": "store_true",
          "default": False,
          "dest": "screenshotOnFail",
          "help": "Take screenshots on all test failures. Set $MOZ_UPLOAD_DIR to a directory for storing the screenshots."
          }],
        [["--quiet"],
         {"action": "store_true",
          "default": False,
          "dest": "quiet",
          "help": "Do not print test log lines unless a failure occurs."
          }],
        [["--pidfile"],
         {"dest": "pidFile",
          "help": "name of the pidfile to generate",
          "default": "",
          }],
        [["--use-test-media-devices"],
         {"action": "store_true",
          "default": False,
          "dest": "useTestMediaDevices",
          "help": "Use test media device drivers for media testing.",
          }],
        [["--gmp-path"],
         {"default": None,
          "dest": "gmp_path",
          "help": "Path to fake GMP plugin. Will be deduced from the binary if not passed.",
          }],
        [["--xre-path"],
         {"dest": "xrePath",
          "default": None,    # individual scripts will set a sane default
          "help": "absolute path to directory containing XRE (probably xulrunner)",
          }],
        [["--symbols-path"],
         {"dest": "symbolsPath",
          "default": None,
          "help": "absolute path to directory containing breakpad symbols, or the URL of a zip file containing symbols",
          }],
        [["--debugger"],
         {"dest": "debugger",
          "help": "use the given debugger to launch the application",
          }],
        [["--debugger-args"],
         {"dest": "debuggerArgs",
          "help": "pass the given args to the debugger _before_ the application on the command line",
          }],
        [["--debugger-interactive"],
         {"action": "store_true",
          "dest": "debuggerInteractive",
          "help": "prevents the test harness from redirecting stdout and stderr for interactive debuggers",
          }],
        [["--max-timeouts"],
         {"type": int,
          "dest": "maxTimeouts",
          "help": "maximum number of timeouts permitted before halting testing",
          "default": None,
          }],
        [["--tag"],
         {"action": "append",
          "dest": "test_tags",
          "default": None,
          "help": "filter out tests that don't have the given tag. Can be "
                  "used multiple times in which case the test must contain "
                  "at least one of the given tags.",
          }],
        [["--enable-cpow-warnings"],
         {"action": "store_true",
          "dest": "enableCPOWWarnings",
          "help": "enable logging of unsafe CPOW usage, which is disabled by default for tests",
          }],
    ]

    def __init__(self, **kwargs):
        ArgumentParser.__init__(self, usage=self.__doc__, **kwargs)
        for option, value in self.mochitest_options:
            # Allocate new lists so references to original don't get mutated.
            # allowing multiple uses within a single process.
            if "default" in value and isinstance(value["default"], list):
                value["default"] = []
            self.add_argument(*option, **value)

    def verifyOptions(self, options, mochitest):
        """ verify correct options and cleanup paths """

        # for test manifest parsing.
        mozinfo.update({"strictContentSandbox": options.strictContentSandbox})
        # for test manifest parsing.
        mozinfo.update({"nested_oop": options.nested_oop})

        if options.app is None:
            if build_obj is not None:
                options.app = build_obj.get_binary_path()
            else:
                self.error(
                    "could not find the application path, --appname must be specified")

        if options.totalChunks is not None and options.thisChunk is None:
            self.error(
                "thisChunk must be specified when totalChunks is specified")

        if options.totalChunks:
            if not 1 <= options.thisChunk <= options.totalChunks:
                self.error("thisChunk must be between 1 and totalChunks")

        if options.chunkByDir and options.chunkByRuntime:
            self.error(
                "can only use one of --chunk-by-dir or --chunk-by-runtime")

        if options.xrePath is None:
            # default xrePath to the app path if not provided
            # but only if an app path was explicitly provided
            if options.app != self.get_default('app'):
                options.xrePath = os.path.dirname(options.app)
                if mozinfo.isMac:
                    options.xrePath = os.path.join(
                        os.path.dirname(
                            options.xrePath),
                        "Resources")
            elif build_obj is not None:
                # otherwise default to dist/bin
                options.xrePath = build_obj.bindir
            else:
                self.error(
                    "could not find xre directory, --xre-path must be specified")

        # allow relative paths
        options.xrePath = mochitest.getFullPath(options.xrePath)
        if options.profilePath:
            options.profilePath = mochitest.getFullPath(options.profilePath)
        options.app = mochitest.getFullPath(options.app)
        if options.dmdPath is not None:
            options.dmdPath = mochitest.getFullPath(options.dmdPath)

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

        if options.symbolsPath and len(
            urlparse(
                options.symbolsPath).scheme) < 2:
            options.symbolsPath = mochitest.getFullPath(options.symbolsPath)

        # Set server information on the options object
        options.webServer = '127.0.0.1'
        options.httpPort = DEFAULT_PORTS['http']
        options.sslPort = DEFAULT_PORTS['https']
        #        options.webSocketPort = DEFAULT_PORTS['ws']
        # <- http://hg.mozilla.org/mozilla-central/file/b871dfb2186f/build/automation.py.in#l30
        options.webSocketPort = str(9988)
        # The default websocket port is incorrect in mozprofile; it is
        # set to the SSL proxy setting. See:
        # see https://bugzilla.mozilla.org/show_bug.cgi?id=916517

        if options.vmwareRecording:
            if not mozinfo.isWin:
                self.error(
                    "use-vmware-recording is only supported on Windows.")
            mochitest.vmwareHelperPath = os.path.join(
                options.utilityPath, VMWARE_RECORDING_HELPER_BASENAME + ".dll")
            if not os.path.exists(mochitest.vmwareHelperPath):
                self.error("%s not found, cannot automate VMware recording." %
                           mochitest.vmwareHelperPath)

        if options.webapprtContent and options.webapprtChrome:
            self.error(
                "Only one of --webapprt-content and --webapprt-chrome may be given.")

        if options.jsdebugger:
            options.extraPrefs += [
                "devtools.debugger.remote-enabled=true",
                "devtools.chrome.enabled=true",
                "devtools.debugger.prompt-connection=false"
            ]
            options.autorun = False

        if options.debugOnFailure and not options.jsdebugger:
            self.error(
                "--debug-on-failure should be used together with --jsdebugger.")

        # Try to guess the testing modules directory.
        # This somewhat grotesque hack allows the buildbot machines to find the
        # modules directory without having to configure the buildbot hosts. This
        # code should never be executed in local runs because the build system
        # should always set the flag that populates this variable. If buildbot ever
        # passes this argument, this code can be deleted.
        if options.testingModulesDir is None:
            possible = os.path.join(here, os.path.pardir, 'modules')

            if os.path.isdir(possible):
                options.testingModulesDir = possible

        # Even if buildbot is updated, we still want this, as the path we pass in
        # to the app must be absolute and have proper slashes.
        if options.testingModulesDir is not None:
            options.testingModulesDir = os.path.normpath(
                options.testingModulesDir)

            if not os.path.isabs(options.testingModulesDir):
                options.testingModulesDir = os.path.abspath(
                    options.testingModulesDir)

            if not os.path.isdir(options.testingModulesDir):
                self.error('--testing-modules-dir not a directory: %s' %
                           options.testingModulesDir)

            options.testingModulesDir = options.testingModulesDir.replace(
                '\\',
                '/')
            if options.testingModulesDir[-1] != '/':
                options.testingModulesDir += '/'

        if options.immersiveMode:
            if not mozinfo.isWin:
                self.error("immersive is only supported on Windows 8 and up.")
            mochitest.immersiveHelperPath = os.path.join(
                options.utilityPath, "metrotestharness.exe")
            if not os.path.exists(mochitest.immersiveHelperPath):
                self.error("%s not found, cannot launch immersive tests." %
                           mochitest.immersiveHelperPath)

        if options.runUntilFailure:
            if not options.repeat:
                options.repeat = 29

        if options.dumpOutputDirectory is None:
            options.dumpOutputDirectory = tempfile.gettempdir()

        if options.dumpAboutMemoryAfterTest or options.dumpDMDAfterTest:
            if not os.path.isdir(options.dumpOutputDirectory):
                self.error('--dump-output-directory not a directory: %s' %
                           options.dumpOutputDirectory)

        if options.useTestMediaDevices:
            if not mozinfo.isLinux:
                self.error(
                    '--use-test-media-devices is only supported on Linux currently')
            for f in ['/usr/bin/gst-launch-0.10', '/usr/bin/pactl']:
                if not os.path.isfile(f):
                    self.error(
                        'Missing binary %s required for '
                        '--use-test-media-devices' % f)

        if options.nested_oop:
            if not options.e10s:
                options.e10s = True
        mozinfo.update({"e10s": options.e10s})  # for test manifest parsing.

        options.leakThresholds = {
            "default": options.defaultLeakThreshold,
            "tab": 25000,  # See dependencies of bug 1051230.
            # GMP rarely gets a log, but when it does, it leaks a little.
            "geckomediaplugin": 20000,
        }

        # Bug 1065098 - The geckomediaplugin process fails to produce a leak
        # log for some reason.
        options.ignoreMissingLeaks = ["geckomediaplugin"]

        # Bug 1091917 - We exit early in tab processes on Windows, so we don't
        # get leak logs yet.
        if mozinfo.isWin:
            options.ignoreMissingLeaks.append("tab")

        # Bug 1121539 - OSX-only intermittent tab process leak in test_ipc.html
        if mozinfo.isMac:
            options.leakThresholds["tab"] = 100000

        return options


class B2GOptions(MochitestOptions):
    b2g_options = [
        [["--b2gpath"],
         {"dest": "b2gPath",
          "help": "path to B2G repo or qemu dir",
          "default": None,
          }],
        [["--desktop"],
         {"action": "store_true",
          "dest": "desktop",
          "help": "Run the tests on a B2G desktop build",
          "default": False,
          }],
        [["--marionette"],
         {"dest": "marionette",
          "help": "host:port to use when connecting to Marionette",
          "default": None,
          }],
        [["--emulator"],
         {"dest": "emulator",
          "help": "Architecture of emulator to use: x86 or arm",
          "default": None,
          }],
        [["--wifi"],
         {"dest": "wifi",
          "help": "Devine wifi configuration for on device mochitest",
          "default": False,
          }],
        [["--sdcard"],
         {"dest": "sdcard",
          "help": "Define size of sdcard: 1MB, 50MB...etc",
          "default": "10MB",
          }],
        [["--no-window"],
         {"action": "store_true",
          "dest": "noWindow",
          "help": "Pass --no-window to the emulator",
          "default": False,
          }],
        [["--adbpath"],
         {"dest": "adbPath",
          "help": "path to adb",
          "default": "adb",
          }],
        [["--deviceIP"],
         {"dest": "deviceIP",
          "help": "ip address of remote device to test",
          "default": None,
          }],
        [["--devicePort"],
         {"dest": "devicePort",
          "help": "port of remote device to test",
          "default": 20701,
          }],
        [["--remote-logfile"],
         {"dest": "remoteLogFile",
          "help": "Name of log file on the device relative to the device root. \
                  PLEASE ONLY USE A FILENAME.",
          "default": None,
          }],
        [["--remote-webserver"],
         {"dest": "remoteWebServer",
          "help": "ip address where the remote web server is hosted at",
          "default": None,
          }],
        [["--http-port"],
         {"dest": "httpPort",
          "help": "ip address where the remote web server is hosted at",
          "default": DEFAULT_PORTS['http'],
          }],
        [["--ssl-port"],
         {"dest": "sslPort",
          "help": "ip address where the remote web server is hosted at",
          "default": DEFAULT_PORTS['https'],
          }],
        [["--gecko-path"],
         {"dest": "geckoPath",
          "help": "the path to a gecko distribution that should \
                   be installed on the emulator prior to test",
          "default": None,
          }],
        [["--profile"],
         {"dest": "profile",
          "help": "for desktop testing, the path to the \
                   gaia profile to use",
          "default": None,
          }],
        [["--logdir"],
         {"dest": "logdir",
          "help": "directory to store log files",
          "default": None,
          }],
        [['--busybox'],
         {"dest": 'busybox',
          "help": "Path to busybox binary to install on device",
          "default": None,
          }],
        [['--profile-data-dir'],
         {"dest": 'profile_data_dir',
          "help": "Path to a directory containing preference and other \
                   data to be installed into the profile",
          "default": os.path.join(here, 'profile_data'),
          }],
    ]

    def __init__(self):
        MochitestOptions.__init__(self)

        for option in self.b2g_options:
            self.add_argument(*option[0], **option[1])

        defaults = {}
        defaults["logFile"] = "mochitest.log"
        defaults["autorun"] = True
        defaults["closeWhenDone"] = True
        defaults["extensionsToExclude"] = ["specialpowers"]
        # See dependencies of bug 1038943.
        defaults["defaultLeakThreshold"] = 5536
        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options):
        if options.remoteWebServer is None:
            if os.name != "nt":
                options.remoteWebServer = moznetwork.get_ip()
            else:
                self.error(
                    "You must specify a --remote-webserver=<ip address>")
        options.webServer = options.remoteWebServer

        if options.geckoPath and not options.emulator:
            self.error(
                "You must specify --emulator if you specify --gecko-path")

        if options.logdir and not options.emulator:
            self.error("You must specify --emulator if you specify --logdir")

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
        options.app = __file__
        tempPort = options.httpPort
        tempSSL = options.sslPort
        tempIP = options.webServer
        options = MochitestOptions.verifyOptions(self, options, mochitest)
        options.webServer = tempIP
        options.app = temp
        options.sslPort = tempSSL
        options.httpPort = tempPort

        # Bug 1071866 - B2G Mochitests do not always produce a leak log.
        options.ignoreMissingLeaks.append("default")

        # Bug 1070068 - Leak logging does not work for tab processes on B2G.
        options.ignoreMissingLeaks.append("tab")

        return options

    def elf_arm(self, filename):
        data = open(filename, 'rb').read(20)
        return data[:4] == "\x7fELF" and ord(data[18]) == 40  # EM_ARM


class RemoteOptions(MochitestOptions):
    remote_options = [
        [["--remote-app-path"],
         {"dest": "remoteAppPath",
          "help": "Path to remote executable relative to device root using \
                   only forward slashes. Either this or app must be specified \
                   but not both.",
          "default": None,
          }],
        [["--deviceIP"],
         {"dest": "deviceIP",
          "help": "ip address of remote device to test",
          "default": None,
          }],
        [["--deviceSerial"],
         {"dest": "deviceSerial",
          "help": "ip address of remote device to test",
          "default": None,
          }],
        [["--dm_trans"],
         {"dest": "dm_trans",
          "default": "sut",
          "help": "the transport to use to communicate with device: \
                   [adb|sut]; default=sut",
          }],
        [["--devicePort"],
         {"dest": "devicePort",
          "type": int,
          "default": 20701,
          "help": "port of remote device to test",
          }],
        [["--remote-product-name"],
         {"dest": "remoteProductName",
          "default": "fennec",
          "help": "The executable's name of remote product to test - either \
                   fennec or firefox, defaults to fennec",
          }],
        [["--remote-logfile"],
         {"dest": "remoteLogFile",
          "default": None,
          "help": "Name of log file on the device relative to the device \
                   root. PLEASE ONLY USE A FILENAME.",
          }],
        [["--remote-webserver"],
         {"dest": "remoteWebServer",
          "default": None,
          "help": "ip address where the remote web server is hosted at",
          }],
        [["--http-port"],
         {"dest": "httpPort",
          "default": DEFAULT_PORTS['http'],
          "help": "http port of the remote web server",
          }],
        [["--ssl-port"],
         {"dest": "sslPort",
          "default": DEFAULT_PORTS['https'],
          "help": "ssl port of the remote web server",
          }],
        [["--robocop-ini"],
         {"dest": "robocopIni",
          "default": "",
          "help": "name of the .ini file containing the list of tests to run",
          }],
        [["--robocop"],
         {"dest": "robocop",
          "default": "",
          "help": "name of the .ini file containing the list of tests to run. \
                   [DEPRECATED- please use --robocop-ini",
          }],
        [["--robocop-apk"],
         {"dest": "robocopApk",
          "default": "",
          "help": "name of the Robocop APK to use for ADB test running",
          }],
        [["--robocop-path"],
         {"dest": "robocopPath",
          "default": "",
          "help": "Path to the folder where robocop.apk is located at. \
                   Primarily used for ADB test running. \
                   [DEPRECATED- please use --robocop-apk]",
          }],
        [["--robocop-ids"],
         {"dest": "robocopIds",
          "default": "",
          "help": "name of the file containing the view ID map \
                   (fennec_ids.txt)",
          }],
        [["--remoteTestRoot"],
         {"dest": "remoteTestRoot",
          "default": None,
          "help": "remote directory to use as test root \
                   (eg. /mnt/sdcard/tests or /data/local/tests)",
          }],
    ]

    def __init__(self, automation, **kwargs):
        self._automation = automation or Automation()
        MochitestOptions.__init__(self)

        for option in self.remote_options:
            self.add_argument(*option[0], **option[1])

        defaults = {}
        defaults["logFile"] = "mochitest.log"
        defaults["autorun"] = True
        defaults["closeWhenDone"] = True
        defaults["utilityPath"] = None
        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options, automation):
        options_logger = logging.getLogger('MochitestRemote')

        if not options.remoteTestRoot:
            options.remoteTestRoot = automation._devicemanager.deviceRoot

        if options.remoteWebServer is None:
            if os.name != "nt":
                options.remoteWebServer = moznetwork.get_ip()
            else:
                options_logger.error(
                    "you must specify a --remote-webserver=<ip address>")
                return None

        options.webServer = options.remoteWebServer

        if (options.dm_trans == 'sut' and options.deviceIP is None):
            options_logger.error(
                "If --dm_trans = sut, you must provide a device IP")
            return None

        if (options.remoteLogFile is None):
            options.remoteLogFile = options.remoteTestRoot + \
                '/logs/mochitest.log'

        if (options.remoteLogFile.count('/') < 1):
            options.remoteLogFile = options.remoteTestRoot + \
                '/' + options.remoteLogFile

        if (options.remoteAppPath and options.app):
            options_logger.error(
                "You cannot specify both the remoteAppPath and the app setting")
            return None
        elif (options.remoteAppPath):
            options.app = options.remoteTestRoot + "/" + options.remoteAppPath
        elif (options.app is None):
            # Neither remoteAppPath nor app are set -- error
            options_logger.error("You must specify either appPath or app")
            return None

        # Only reset the xrePath if it wasn't provided
        if (options.xrePath is None):
            options.xrePath = options.utilityPath

        if (options.pidFile != ""):
            f = open(options.pidFile, 'w')
            f.write("%s" % os.getpid())
            f.close()

        # Robocop specific deprecated options.
        if options.robocop:
            if options.robocopIni:
                options_logger.error(
                    "can not use deprecated --robocop and replacement --robocop-ini together")
                return None
            options.robocopIni = options.robocop
            del options.robocop

        if options.robocopPath:
            if options.robocopApk:
                options_logger.error(
                    "can not use deprecated --robocop-path and replacement --robocop-apk together")
                return None
            options.robocopApk = os.path.join(
                options.robocopPath,
                'robocop.apk')
            del options.robocopPath

        # Robocop specific options
        if options.robocopIni != "":
            if not os.path.exists(options.robocopIni):
                options_logger.error(
                    "Unable to find specified robocop .ini manifest '%s'" %
                    options.robocopIni)
                return None
            options.robocopIni = os.path.abspath(options.robocopIni)

        if options.robocopApk != "":
            if not os.path.exists(options.robocopApk):
                options_logger.error(
                    "Unable to find robocop APK '%s'" %
                    options.robocopApk)
                return None
            options.robocopApk = os.path.abspath(options.robocopApk)

        if options.robocopIds != "":
            if not os.path.exists(options.robocopIds):
                options_logger.error(
                    "Unable to find specified robocop IDs file '%s'" %
                    options.robocopIds)
                return None
            options.robocopIds = os.path.abspath(options.robocopIds)

        # allow us to keep original application around for cleanup while
        # running robocop via 'am'
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

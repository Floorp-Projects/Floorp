# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from abc import ABCMeta, abstractmethod, abstractproperty
from argparse import ArgumentParser, SUPPRESS
from urlparse import urlparse
import os
import tempfile

from droid import DroidADB, DroidSUT
from mozlog import structured
from mozprofile import DEFAULT_PORTS
import mozinfo
import moznetwork


here = os.path.abspath(os.path.dirname(__file__))

try:
    from mozbuild.base import (
        MozbuildObject,
        MachCommandConditions as conditions,
    )
    build_obj = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build_obj = None
    conditions = None


VMWARE_RECORDING_HELPER_BASENAME = "vmwarerecordinghelper"


class ArgumentContainer():
    __metaclass__ = ABCMeta

    @abstractproperty
    def args(self):
        pass

    @abstractproperty
    def defaults(self):
        pass

    @abstractmethod
    def validate(self, parser, args, context):
        pass

    def get_full_path(self, path, cwd):
        """Get an absolute path relative to cwd."""
        return os.path.normpath(os.path.join(cwd, os.path.expanduser(path)))


class MochitestArguments(ArgumentContainer):
    """General mochitest arguments."""

    LOG_LEVELS = ("DEBUG", "INFO", "WARNING", "ERROR", "FATAL")
    LEVEL_STRING = ", ".join(LOG_LEVELS)

    args = [
        [["--keep-open"],
         {"action": "store_false",
          "dest": "closeWhenDone",
          "default": True,
          "help": "Always keep the browser open after tests complete.",
          }],
        [["--appname"],
         {"dest": "app",
          "default": None,
          "help": "Override the default binary used to run tests with the path provided, e.g "
                  "/usr/bin/firefox. If you have run ./mach package beforehand, you can "
                  "specify 'dist' to run tests against the distribution bundle's binary.",
          "suppress": True,
          }],
        [["--utility-path"],
         {"dest": "utilityPath",
          "default": build_obj.bindir if build_obj is not None else None,
          "help": "absolute path to directory containing utility programs "
                  "(xpcshell, ssltunnel, certutil)",
          "suppress": True,
          }],
        [["--certificate-path"],
         {"dest": "certPath",
          "default": None,
          "help": "absolute path to directory containing certificate store to use testing profile",
          "suppress": True,
          }],
        [["--no-autorun"],
         {"action": "store_false",
          "dest": "autorun",
          "default": True,
          "help": "Do not start running tests automatically.",
          }],
        [["--timeout"],
         {"type": int,
          "default": None,
          "help": "The per-test timeout in seconds (default: 60 seconds).",
          }],
        [["--max-timeouts"],
         {"type": int,
          "dest": "maxTimeouts",
          "default": None,
          "help": "The maximum number of timeouts permitted before halting testing.",
          }],
        [["--total-chunks"],
         {"type": int,
          "dest": "totalChunks",
          "help": "Total number of chunks to split tests into.",
          "default": None,
          }],
        [["--this-chunk"],
         {"type": int,
          "dest": "thisChunk",
          "help": "If running tests by chunks, the chunk number to run.",
          "default": None,
          }],
        [["--chunk-by-runtime"],
         {"action": "store_true",
          "dest": "chunkByRuntime",
          "help": "Group tests such that each chunk has roughly the same runtime.",
          "default": False,
          }],
        [["--chunk-by-dir"],
         {"type": int,
          "dest": "chunkByDir",
          "help": "Group tests together in the same chunk that are in the same top "
                  "chunkByDir directories.",
          "default": 0,
          }],
        [["--run-by-dir"],
         {"action": "store_true",
          "dest": "runByDir",
          "help": "Run each directory in a single browser instance with a fresh profile.",
          "default": False,
          }],
        [["--shuffle"],
         {"action": "store_true",
          "help": "Shuffle execution order of tests.",
          "default": False,
          }],
        [["--console-level"],
         {"dest": "consoleLevel",
          "choices": LOG_LEVELS,
          "default": "INFO",
          "help": "One of %s to determine the level of console logging." % LEVEL_STRING,
          "suppress": True,
          }],
        [["--chrome"],
         {"action": "store_true",
          "default": False,
          "help": "Run chrome mochitests.",
          "suppress": True,
          }],
        [["--ipcplugins"],
         {"action": "store_true",
          "dest": "ipcplugins",
          "help": "Run ipcplugins mochitests.",
          "default": False,
          "suppress": True,
          }],
        [["--test-path"],
         {"dest": "testPath",
          "default": "",
          "help": "Run the given test or recursively run the given directory of tests.",
          # if running from mach, a test_paths arg is exposed instead
          "suppress": build_obj is not None,
          }],
        [["--bisect-chunk"],
         {"dest": "bisectChunk",
          "default": None,
          "help": "Specify the failing test name to find the previous tests that may be "
                  "causing the failure.",
          }],
        [["--start-at"],
         {"dest": "startAt",
          "default": "",
          "help": "Start running the test sequence at this test.",
          }],
        [["--end-at"],
         {"dest": "endAt",
          "default": "",
          "help": "Stop running the test sequence at this test.",
          }],
        [["--browser-chrome"],
         {"action": "store_true",
          "dest": "browserChrome",
          "default": False,
          "help": "run browser chrome Mochitests",
          "suppress": True,
          }],
        [["--subsuite"],
         {"default": None,
          "help": "Subsuite of tests to run. Unlike tags, subsuites also remove tests from "
                  "the default set. Only one can be specified at once.",
          }],
        [["--jetpack-package"],
         {"action": "store_true",
          "dest": "jetpackPackage",
          "help": "Run jetpack package tests.",
          "default": False,
          "suppress": True,
          }],
        [["--jetpack-addon"],
         {"action": "store_true",
          "dest": "jetpackAddon",
          "help": "Run jetpack addon tests.",
          "default": False,
          "suppress": True,
          }],
        [["--webapprt-content"],
         {"action": "store_true",
          "dest": "webapprtContent",
          "help": "Run WebappRT content tests.",
          "default": False,
          "suppress": True,
          }],
        [["--webapprt-chrome"],
         {"action": "store_true",
          "dest": "webapprtChrome",
          "help": "Run WebappRT chrome tests.",
          "default": False,
          "suppress": True,
          }],
        [["--a11y"],
         {"action": "store_true",
          "help": "Run accessibility Mochitests.",
          "default": False,
          "suppress": True,
          }],
        [["--setenv"],
         {"action": "append",
          "dest": "environment",
          "metavar": "NAME=VALUE",
          "default": [],
          "help": "Sets the given variable in the application's environment.",
          }],
        [["--exclude-extension"],
         {"action": "append",
          "dest": "extensionsToExclude",
          "default": [],
          "help": "Excludes the given extension from being installed in the test profile.",
          "suppress": True,
          }],
        [["--browser-arg"],
         {"action": "append",
          "dest": "browserArgs",
          "default": [],
          "help": "Provides an argument to the test application (e.g Firefox).",
          "suppress": True,
          }],
        [["--leak-threshold"],
         {"type": int,
          "dest": "defaultLeakThreshold",
          "default": 0,
          "help": "Fail if the number of bytes leaked in default processes through "
                  "refcounted objects (or bytes in classes with MOZ_COUNT_CTOR and "
                  "MOZ_COUNT_DTOR) is greater than the given number.",
          "suppress": True,
          }],
        [["--fatal-assertions"],
         {"action": "store_true",
          "dest": "fatalAssertions",
          "default": False,
          "help": "Abort testing whenever an assertion is hit (requires a debug build to "
                  "be effective).",
          "suppress": True,
          }],
        [["--extra-profile-file"],
         {"action": "append",
          "dest": "extraProfileFiles",
          "default": [],
          "help": "Copy specified files/dirs to testing profile. Can be specified more "
                  "than once.",
          "suppress": True,
          }],
        [["--install-extension"],
         {"action": "append",
          "dest": "extensionsToInstall",
          "default": [],
          "help": "Install the specified extension in the testing profile. Can be a path "
                  "to a .xpi file.",
          }],
        [["--profile-path"],
         {"dest": "profilePath",
          "default": None,
          "help": "Directory where the profile will be stored. This directory will be "
                  "deleted after the tests are finished.",
          "suppress": True,
          }],
        [["--testing-modules-dir"],
         {"dest": "testingModulesDir",
          "default": None,
          "help": "Directory where testing-only JS modules are located.",
          "suppress": True,
          }],
        [["--use-vmware-recording"],
         {"action": "store_true",
          "dest": "vmwareRecording",
          "default": False,
          "help": "Enables recording while the application is running inside a VMware "
                  "Workstation 7.0 or later VM.",
          "suppress": True,
          }],
        [["--repeat"],
         {"type": int,
          "default": 0,
          "help": "Repeat the tests the given number of times.",
          }],
        [["--run-until-failure"],
         {"action": "store_true",
          "dest": "runUntilFailure",
          "default": False,
          "help": "Run tests repeatedly but stop the first time a test fails. Default cap "
                  "is 30 runs, which can be overridden with the --repeat parameter.",
          }],
        [["--manifest"],
         {"dest": "manifestFile",
          "default": None,
          "help": "Path to a manifestparser (.ini formatted) manifest of tests to run.",
          "suppress": True,
          }],
        [["--testrun-manifest-file"],
         {"dest": "testRunManifestFile",
          "default": 'tests.json',
          "help": "Overrides the default filename of the tests.json manifest file that is "
                  "generated by the harness and used by SimpleTest. Only useful when running "
                  "multiple test runs simulatenously on the same machine.",
          "suppress": True,
          }],
        [["--failure-file"],
         {"dest": "failureFile",
          "default": None,
          "help": "Filename of the output file where we can store a .json list of failures "
                  "to be run in the future with --run-only-tests.",
          "suppress": True,
          }],
        [["--run-slower"],
         {"action": "store_true",
          "dest": "runSlower",
          "default": False,
          "help": "Delay execution between tests.",
          }],
        [["--metro-immersive"],
         {"action": "store_true",
          "dest": "immersiveMode",
          "default": False,
          "help": "Launches tests in an immersive browser.",
          "suppress": True,
          }],
        [["--httpd-path"],
         {"dest": "httpdPath",
          "default": None,
          "help": "Path to the httpd.js file.",
          "suppress": True,
          }],
        [["--setpref"],
         {"action": "append",
          "metavar": "PREF=VALUE",
          "default": [],
          "dest": "extraPrefs",
          "help": "Defines an extra user preference.",
          }],
        [["--jsdebugger"],
         {"action": "store_true",
          "default": False,
          "help": "Start the browser JS debugger before running the test. Implies --no-autorun.",
          }],
        [["--debug-on-failure"],
         {"action": "store_true",
          "default": False,
          "dest": "debugOnFailure",
          "help": "Breaks execution and enters the JS debugger on a test failure. Should "
                  "be used together with --jsdebugger."
          }],
        [["--e10s"],
         {"action": "store_true",
          "default": False,
          "help": "Run tests with electrolysis preferences and test filtering enabled.",
          }],
        [["--strict-content-sandbox"],
         {"action": "store_true",
          "default": False,
          "dest": "strictContentSandbox",
          "help": "Run tests with a more strict content sandbox (Windows only).",
          "suppress": not mozinfo.isWin,
          }],
        [["--nested_oop"],
         {"action": "store_true",
          "default": False,
          "help": "Run tests with nested_oop preferences and test filtering enabled.",
          }],
        [["--dmd"],
         {"action": "store_true",
          "default": False,
          "help": "Run tests with DMD active.",
          }],
        [["--dmd-path"],
         {"default": None,
          "dest": "dmdPath",
          "help": "Specifies the path to the directory containing the shared library for DMD.",
          "suppress": True,
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
          "help": "Dump an about:memory log after each test in the directory specified "
                  "by --dump-output-directory.",
          }],
        [["--dump-dmd-after-test"],
         {"action": "store_true",
          "default": False,
          "dest": "dumpDMDAfterTest",
          "help": "Dump a DMD log after each test in the directory specified "
                  "by --dump-output-directory.",
          }],
        [["--slowscript"],
         {"action": "store_true",
          "default": False,
          "help": "Do not set the JS_DISABLE_SLOW_SCRIPT_SIGNALS env variable; "
                  "when not set, recoverable but misleading SIGSEGV instances "
                  "may occur in Ion/Odin JIT code.",
          }],
        [["--screenshot-on-fail"],
         {"action": "store_true",
          "default": False,
          "dest": "screenshotOnFail",
          "help": "Take screenshots on all test failures. Set $MOZ_UPLOAD_DIR to a directory "
                  "for storing the screenshots."
          }],
        [["--quiet"],
         {"action": "store_true",
          "dest": "quiet",
          "default": False,
          "help": "Do not print test log lines unless a failure occurs.",
          }],
        [["--pidfile"],
         {"dest": "pidFile",
          "default": "",
          "help": "Name of the pidfile to generate.",
          "suppress": True,
          }],
        [["--use-test-media-devices"],
         {"action": "store_true",
          "default": False,
          "dest": "useTestMediaDevices",
          "help": "Use test media device drivers for media testing.",
          }],
        [["--gmp-path"],
         {"default": None,
          "help": "Path to fake GMP plugin. Will be deduced from the binary if not passed.",
          "suppress": True,
          }],
        [["--xre-path"],
         {"dest": "xrePath",
          "default": None,    # individual scripts will set a sane default
          "help": "Absolute path to directory containing XRE (probably xulrunner).",
          "suppress": True,
          }],
        [["--symbols-path"],
         {"dest": "symbolsPath",
          "default": None,
          "help": "Absolute path to directory containing breakpad symbols, or the URL of a "
                  "zip file containing symbols",
          "suppress": True,
          }],
        [["--debugger"],
         {"default": None,
          "help": "Debugger binary to run tests in. Program name or path.",
          }],
        [["--debugger-args"],
         {"dest": "debuggerArgs",
          "default": None,
          "help": "Arguments to pass to the debugger.",
          }],
        [["--debugger-interactive"],
         {"action": "store_true",
          "dest": "debuggerInteractive",
          "default": None,
          "help": "Prevents the test harness from redirecting stdout and stderr for "
                  "interactive debuggers.",
          "suppress": True,
          }],
        [["--tag"],
         {"action": "append",
          "dest": "test_tags",
          "default": None,
          "help": "Filter out tests that don't have the given tag. Can be used multiple "
                  "times in which case the test must contain at least one of the given tags.",
          }],
        [["--enable-cpow-warnings"],
         {"action": "store_true",
          "dest": "enableCPOWWarnings",
          "help": "Enable logging of unsafe CPOW usage, which is disabled by default for tests",
          "suppress": True,
          }],
    ]

    defaults = {
        # Bug 1065098 - The geckomediaplugin process fails to produce a leak
        # log for some reason.
        'ignoreMissingLeaks': ["geckomediaplugin"],

        # Set server information on the args object
        'webServer': '127.0.0.1',
        'httpPort': DEFAULT_PORTS['http'],
        'sslPort': DEFAULT_PORTS['https'],
        'webSocketPort': '9988',
        # The default websocket port is incorrect in mozprofile; it is
        # set to the SSL proxy setting. See:
        # see https://bugzilla.mozilla.org/show_bug.cgi?id=916517
        # args.webSocketPort = DEFAULT_PORTS['ws']
    }

    def validate(self, parser, options, context):
        """Validate generic options."""

        # for test manifest parsing.
        mozinfo.update({"strictContentSandbox": options.strictContentSandbox})
        # for test manifest parsing.
        mozinfo.update({"nested_oop": options.nested_oop})

        # b2g and android don't use 'app' the same way, so skip validation
        if parser.app not in ('b2g', 'android'):
            if options.app is None:
                if build_obj:
                    options.app = build_obj.get_binary_path()
                else:
                    parser.error(
                        "could not find the application path, --appname must be specified")
            elif options.app == "dist" and build_obj:
                options.app = build_obj.get_binary_path(where='staged-package')

            options.app = self.get_full_path(options.app, parser.oldcwd)
            if not os.path.exists(options.app):
                parser.error("Error: Path {} doesn't exist. Are you executing "
                             "$objdir/_tests/testing/mochitest/runtests.py?".format(
                                 options.app))

        if options.gmp_path is None and options.app and build_obj:
            # Need to fix the location of gmp_fake which might not be shipped in the binary
            gmp_modules = (
                ('gmp-fake', '1.0'),
                ('gmp-clearkey', '0.1'),
                ('gmp-fakeopenh264', '1.0')
            )
            options.gmp_path = os.pathsep.join(
                os.path.join(build_obj.bindir, *p) for p in gmp_modules)

        if options.totalChunks is not None and options.thisChunk is None:
            parser.error(
                "thisChunk must be specified when totalChunks is specified")

        if options.totalChunks:
            if not 1 <= options.thisChunk <= options.totalChunks:
                parser.error("thisChunk must be between 1 and totalChunks")

        if options.chunkByDir and options.chunkByRuntime:
            parser.error(
                "can only use one of --chunk-by-dir or --chunk-by-runtime")

        if options.xrePath is None:
            # default xrePath to the app path if not provided
            # but only if an app path was explicitly provided
            if options.app != parser.get_default('app'):
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
                parser.error(
                    "could not find xre directory, --xre-path must be specified")

        # allow relative paths
        options.xrePath = self.get_full_path(options.xrePath, parser.oldcwd)
        if options.profilePath:
            options.profilePath = self.get_full_path(options.profilePath, parser.oldcwd)

        if options.dmdPath:
            options.dmdPath = self.get_full_path(options.dmdPath, parser.oldcwd)

        if options.dmd and not options.dmdPath:
            if build_obj:
                options.dmdPath = build_obj.bin_dir
            else:
                parser.error(
                    "could not find dmd libraries, specify them with --dmd-path")

        if options.utilityPath:
            options.utilityPath = self.get_full_path(options.utilityPath, parser.oldcwd)

        if options.certPath:
            options.certPath = self.get_full_path(options.certPath, parser.oldcwd)
        elif build_obj:
            options.certPath = os.path.join(build_obj.topsrcdir, 'build', 'pgo', 'certs')

        if options.symbolsPath and len(urlparse(options.symbolsPath).scheme) < 2:
            options.symbolsPath = self.get_full_path(options.symbolsPath, parser.oldcwd)
        elif not options.symbolsPath and build_obj:
            options.symbolsPath = os.path.join(build_obj.distdir, 'crashreporter-symbols')

        if options.vmwareRecording:
            if not mozinfo.isWin:
                parser.error(
                    "use-vmware-recording is only supported on Windows.")
            options.vmwareHelperPath = os.path.join(
                options.utilityPath, VMWARE_RECORDING_HELPER_BASENAME + ".dll")
            if not os.path.exists(options.vmwareHelperPath):
                parser.error("%s not found, cannot automate VMware recording." %
                             options.vmwareHelperPath)

        if options.webapprtContent and options.webapprtChrome:
            parser.error(
                "Only one of --webapprt-content and --webapprt-chrome may be given.")

        if options.jsdebugger:
            options.extraPrefs += [
                "devtools.debugger.remote-enabled=true",
                "devtools.chrome.enabled=true",
                "devtools.debugger.prompt-connection=false"
            ]
            options.autorun = False

        if options.debugOnFailure and not options.jsdebugger:
            parser.error(
                "--debug-on-failure requires --jsdebugger.")

        if options.debuggerArgs and not options.debugger:
            parser.error(
                "--debugger-args requires --debugger.")

        if options.testingModulesDir is None:
            if build_obj:
                options.testingModulesDir = os.path.join(
                    build_obj.topobjdir, '_tests', 'modules')
            else:
                # Try to guess the testing modules directory.
                # This somewhat grotesque hack allows the buildbot machines to find the
                # modules directory without having to configure the buildbot hosts. This
                # code should never be executed in local runs because the build system
                # should always set the flag that populates this variable. If buildbot ever
                # passes this argument, this code can be deleted.
                possible = os.path.join(here, os.path.pardir, 'modules')

                if os.path.isdir(possible):
                    options.testingModulesDir = possible

        if build_obj:
            plugins_dir = os.path.join(build_obj.distdir, 'plugins')
            if plugins_dir not in options.extraProfileFiles:
                options.extraProfileFiles.append(plugins_dir)

        # Even if buildbot is updated, we still want this, as the path we pass in
        # to the app must be absolute and have proper slashes.
        if options.testingModulesDir is not None:
            options.testingModulesDir = os.path.normpath(
                options.testingModulesDir)

            if not os.path.isabs(options.testingModulesDir):
                options.testingModulesDir = os.path.abspath(
                    options.testingModulesDir)

            if not os.path.isdir(options.testingModulesDir):
                parser.error('--testing-modules-dir not a directory: %s' %
                             options.testingModulesDir)

            options.testingModulesDir = options.testingModulesDir.replace(
                '\\',
                '/')
            if options.testingModulesDir[-1] != '/':
                options.testingModulesDir += '/'

        if options.immersiveMode:
            if not mozinfo.isWin:
                parser.error("immersive is only supported on Windows 8 and up.")
            options.immersiveHelperPath = os.path.join(
                options.utilityPath, "metrotestharness.exe")
            if not os.path.exists(options.immersiveHelperPath):
                parser.error("%s not found, cannot launch immersive tests." %
                             options.immersiveHelperPath)

        if options.runUntilFailure:
            if not options.repeat:
                options.repeat = 29

        if options.dumpOutputDirectory is None:
            options.dumpOutputDirectory = tempfile.gettempdir()

        if options.dumpAboutMemoryAfterTest or options.dumpDMDAfterTest:
            if not os.path.isdir(options.dumpOutputDirectory):
                parser.error('--dump-output-directory not a directory: %s' %
                             options.dumpOutputDirectory)

        if options.useTestMediaDevices:
            if not mozinfo.isLinux:
                parser.error(
                    '--use-test-media-devices is only supported on Linux currently')
            for f in ['/usr/bin/gst-launch-0.10', '/usr/bin/pactl']:
                if not os.path.isfile(f):
                    parser.error(
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

        # Bug 1091917 - We exit early in tab processes on Windows, so we don't
        # get leak logs yet.
        if mozinfo.isWin:
            options.ignoreMissingLeaks.append("tab")

        # Bug 1121539 - OSX-only intermittent tab process leak in test_ipc.html
        if mozinfo.isMac:
            options.leakThresholds["tab"] = 100000

        return options


class B2GArguments(ArgumentContainer):
    """B2G specific arguments."""

    args = [
        [["--b2gpath"],
         {"dest": "b2gPath",
          "default": None,
          "help": "Path to B2G repo or QEMU directory.",
          "suppress": True,
          }],
        [["--desktop"],
         {"action": "store_true",
          "default": False,
          "help": "Run the tests on a B2G desktop build.",
          "suppress": True,
          }],
        [["--marionette"],
         {"default": None,
          "help": "host:port to use when connecting to Marionette",
          }],
        [["--emulator"],
         {"default": None,
          "help": "Architecture of emulator to use, x86 or arm",
          "suppress": True,
          }],
        [["--wifi"],
         {"default": False,
          "help": "Devine wifi configuration for on device mochitest",
          "suppress": True,
          }],
        [["--sdcard"],
         {"default": "10MB",
          "help": "Define size of sdcard: 1MB, 50MB...etc",
          }],
        [["--no-window"],
         {"action": "store_true",
          "dest": "noWindow",
          "default": False,
          "help": "Pass --no-window to the emulator",
          }],
        [["--adbpath"],
         {"dest": "adbPath",
          "default": "adb",
          "help": "Path to adb binary.",
          "suppress": True,
          }],
        [["--deviceIP"],
         {"dest": "deviceIP",
          "default": None,
          "help": "IP address of remote device to test.",
          "suppress": True,
          }],
        [["--devicePort"],
         {"default": 20701,
          "help": "port of remote device to test",
          "suppress": True,
          }],
        [["--remote-logfile"],
         {"dest": "remoteLogFile",
          "default": None,
          "help": "Name of log file on the device relative to the device root. "
                  "PLEASE ONLY USE A FILENAME.",
          "suppress": True,
          }],
        [["--remote-webserver"],
         {"dest": "remoteWebServer",
          "default": None,
          "help": "IP address where the remote web server is hosted.",
          "suppress": True,
          }],
        [["--http-port"],
         {"dest": "httpPort",
          "default": DEFAULT_PORTS['http'],
          "help": "Port used for http on the remote web server.",
          "suppress": True,
          }],
        [["--ssl-port"],
         {"dest": "sslPort",
          "default": DEFAULT_PORTS['https'],
          "help": "Port used for https on the remote web server.",
          "suppress": True,
          }],
        [["--gecko-path"],
         {"dest": "geckoPath",
          "default": None,
          "help": "The path to a gecko distribution that should be installed on the emulator "
                  "prior to test.",
          "suppress": True,
          }],
        [["--profile"],
         {"dest": "profile",
          "default": None,
          "help": "For desktop testing, the path to the gaia profile to use.",
          }],
        [["--logdir"],
         {"dest": "logdir",
          "default": None,
          "help": "Directory to store log files.",
          }],
        [['--busybox'],
         {"dest": 'busybox',
          "default": None,
          "help": "Path to busybox binary to install on device.",
          }],
        [['--profile-data-dir'],
         {"dest": 'profile_data_dir',
          "default": os.path.join(here, 'profile_data'),
          "help": "Path to a directory containing preference and other data to be installed "
                  "into the profile.",
          "suppress": True,
          }],
    ]

    defaults = {
        'logFile': 'mochitest.log',
        'extensionsToExclude': ['specialpowers'],
        # See dependencies of bug 1038943.
        'defaultLeakThreshold': 5536,
    }

    def validate(self, parser, options, context):
        """Validate b2g options."""

        if options.desktop and not options.app:
            if not (build_obj and conditions.is_b2g_desktop(build_obj)):
                parser.error(
                    "--desktop specified, but no b2g desktop build detected! Either "
                    "build for b2g desktop, or point --appname to a b2g desktop binary.")
        elif build_obj and conditions.is_b2g_desktop(build_obj):
            options.desktop = True
            if not options.app:
                options.app = build_obj.get_binary_path()
                if not options.app.endswith('-bin'):
                    options.app = '%s-bin' % options.app
                if not os.path.isfile(options.app):
                    options.app = options.app[:-len('-bin')]

        if options.remoteWebServer is None:
            if os.name != "nt":
                options.remoteWebServer = moznetwork.get_ip()
            else:
                parser.error(
                    "You must specify a --remote-webserver=<ip address>")
        options.webServer = options.remoteWebServer

        if not options.b2gPath and hasattr(context, 'b2g_home'):
            options.b2gPath = context.b2g_home

        if hasattr(context, 'device_name') and not options.emulator:
            if context.device_name.startswith('emulator'):
                options.emulator = 'x86' if 'x86' in context.device_name else 'arm'

        if options.geckoPath and not options.emulator:
            parser.error(
                "You must specify --emulator if you specify --gecko-path")

        if options.logdir and not options.emulator:
            parser.error("You must specify --emulator if you specify --logdir")
        elif not options.logdir and options.emulator and build_obj:
            options.logdir = os.path.join(
                build_obj.topobjdir, '_tests', 'testing', 'mochitest')

        if hasattr(context, 'xre_path'):
            options.xrePath = context.xre_path

        if not os.path.isdir(options.xrePath):
            parser.error("--xre-path '%s' is not a directory" % options.xrePath)

        xpcshell = os.path.join(options.xrePath, 'xpcshell')
        if not os.access(xpcshell, os.F_OK):
            parser.error('xpcshell not found at %s' % xpcshell)

        if self.elf_arm(xpcshell):
            parser.error('--xre-path points to an ARM version of xpcshell; it '
                         'should instead point to a version that can run on '
                         'your desktop')

        if not options.httpdPath and build_obj:
            options.httpdPath = os.path.join(
                build_obj.topobjdir, '_tests', 'testing', 'mochitest')

        # Bug 1071866 - B2G Mochitests do not always produce a leak log.
        options.ignoreMissingLeaks.append("default")
        # Bug 1070068 - Leak logging does not work for tab processes on B2G.
        options.ignoreMissingLeaks.append("tab")

        if options.pidFile != "":
            f = open(options.pidFile, 'w')
            f.write("%s" % os.getpid())
            f.close()

        return options

    def elf_arm(self, filename):
        data = open(filename, 'rb').read(20)
        return data[:4] == "\x7fELF" and ord(data[18]) == 40  # EM_ARM


class AndroidArguments(ArgumentContainer):
    """Android specific arguments."""

    args = [
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
         {"choices": ["adb", "sut"],
          "default": "adb",
          "help": "The transport to use for communication with the device [default: adb].",
          "suppress": True,
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
          "suppress": True,
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
          "suppress": True,
          }],
        [["--ssl-port"],
         {"dest": "sslPort",
          "default": DEFAULT_PORTS['https'],
          "help": "ssl port of the remote web server",
          "suppress": True,
          }],
        [["--robocop-ini"],
         {"dest": "robocopIni",
          "default": "",
          "help": "name of the .ini file containing the list of tests to run",
          }],
        [["--robocop-apk"],
         {"dest": "robocopApk",
          "default": "",
          "help": "name of the Robocop APK to use for ADB test running",
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
          "suppress": True,
          }],
    ]

    defaults = {
        'dm': None,
        'logFile': 'mochitest.log',
        'utilityPath': None,
    }

    def validate(self, parser, options, context):
        """Validate android options."""

        if build_obj:
            options.log_mach = '-'

        if options.dm_trans == "adb":
            if options.deviceIP:
                options.dm = DroidADB(
                    options.deviceIP,
                    options.devicePort,
                    deviceRoot=options.remoteTestRoot)
            elif options.deviceSerial:
                options.dm = DroidADB(
                    None,
                    None,
                    deviceSerial=options.deviceSerial,
                    deviceRoot=options.remoteTestRoot)
            else:
                options.dm = DroidADB(deviceRoot=options.remoteTestRoot)
        elif options.dm_trans == 'sut':
            if options.deviceIP is None:
                parser.error(
                    "If --dm_trans = sut, you must provide a device IP")

            options.dm = DroidSUT(
                options.deviceIP,
                options.devicePort,
                deviceRoot=options.remoteTestRoot)

        if not options.remoteTestRoot:
            options.remoteTestRoot = options.dm.deviceRoot

        if options.remoteWebServer is None:
            if os.name != "nt":
                options.remoteWebServer = moznetwork.get_ip()
            else:
                parser.error(
                    "you must specify a --remote-webserver=<ip address>")

        options.webServer = options.remoteWebServer

        if options.remoteLogFile is None:
            options.remoteLogFile = options.remoteTestRoot + \
                '/logs/mochitest.log'

        if options.remoteLogFile.count('/') < 1:
            options.remoteLogFile = options.remoteTestRoot + \
                '/' + options.remoteLogFile

        if options.remoteAppPath and options.app:
            parser.error(
                "You cannot specify both the remoteAppPath and the app setting")
        elif options.remoteAppPath:
            options.app = options.remoteTestRoot + "/" + options.remoteAppPath
        elif options.app is None:
            if build_obj:
                options.app = build_obj.substs['ANDROID_PACKAGE_NAME']
            else:
                # Neither remoteAppPath nor app are set -- error
                parser.error("You must specify either appPath or app")

        if build_obj and 'MOZ_HOST_BIN' in os.environ:
            options.xrePath = os.environ['MOZ_HOST_BIN']

        # Only reset the xrePath if it wasn't provided
        if options.xrePath is None:
            options.xrePath = options.utilityPath

        if options.pidFile != "":
            f = open(options.pidFile, 'w')
            f.write("%s" % os.getpid())
            f.close()

        # Robocop specific options
        if options.robocopIni != "":
            if not os.path.exists(options.robocopIni):
                parser.error(
                    "Unable to find specified robocop .ini manifest '%s'" %
                    options.robocopIni)
            options.robocopIni = os.path.abspath(options.robocopIni)

            if not options.robocopApk and build_obj:
                options.robocopApk = os.path.join(build_obj.topobjdir, 'build', 'mobile',
                                                  'robocop', 'robocop-debug.apk')

        if options.robocopApk != "":
            if not os.path.exists(options.robocopApk):
                parser.error(
                    "Unable to find robocop APK '%s'" %
                    options.robocopApk)
            options.robocopApk = os.path.abspath(options.robocopApk)

        if options.robocopIds != "":
            if not os.path.exists(options.robocopIds):
                parser.error(
                    "Unable to find specified robocop IDs file '%s'" %
                    options.robocopIds)
            options.robocopIds = os.path.abspath(options.robocopIds)

        # allow us to keep original application around for cleanup while
        # running robocop via 'am'
        options.remoteappname = options.app
        return options


container_map = {
    'generic': [MochitestArguments],
    'b2g': [MochitestArguments, B2GArguments],
    'android': [MochitestArguments, AndroidArguments],
}


class MochitestArgumentParser(ArgumentParser):
    """
    Usage instructions for runtests.py.

    All arguments are optional.
    If --chrome is specified, chrome tests will be run instead of web content tests.
    If --browser-chrome is specified, browser-chrome tests will be run instead of web content tests.
    See <http://mochikit.com/doc/html/MochiKit/Logging.html> for details on the logging levels.
    """

    _containers = None
    context = {}

    def __init__(self, app=None, **kwargs):
        ArgumentParser.__init__(self, usage=self.__doc__, conflict_handler='resolve', **kwargs)

        self.oldcwd = os.getcwd()
        self.app = app
        if not self.app and build_obj:
            if conditions.is_android(build_obj):
                self.app = 'android'
            elif conditions.is_b2g(build_obj) or conditions.is_b2g_desktop(build_obj):
                self.app = 'b2g'
        if not self.app:
            # platform can't be determined and app wasn't specified explicitly,
            # so just use generic arguments and hope for the best
            self.app = 'generic'

        if self.app not in container_map:
            self.error("Unrecognized app '{}'! Must be one of: {}".format(
                self.app, ', '.join(container_map.keys())))

        defaults = {}
        for container in self.containers:
            defaults.update(container.defaults)
            group = self.add_argument_group(container.__class__.__name__, container.__doc__)

            for cli, kwargs in container.args:
                # Allocate new lists so references to original don't get mutated.
                # allowing multiple uses within a single process.
                if "default" in kwargs and isinstance(kwargs['default'], list):
                    kwargs["default"] = []

                if 'suppress' in kwargs:
                    if kwargs['suppress']:
                        kwargs['help'] = SUPPRESS
                    del kwargs['suppress']

                group.add_argument(*cli, **kwargs)

        self.set_defaults(**defaults)
        structured.commandline.add_logging_group(self)

    @property
    def containers(self):
        if self._containers:
            return self._containers

        containers = container_map[self.app]
        self._containers = [c() for c in containers]
        return self._containers

    def validate(self, args):
        for container in self.containers:
            args = container.validate(self, args, self.context)
        return args

    def parse_args(self, *args, **kwargs):
        return self.validate(ArgumentParser.parse_args(self, *args, **kwargs))

    def parse_known_args(self, *args, **kwargs):
        args, remainder = ArgumentParser.parse_known_args(self, *args, **kwargs)
        return (self.validate(args), remainder)

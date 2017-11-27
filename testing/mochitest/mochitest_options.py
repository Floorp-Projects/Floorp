# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from abc import ABCMeta, abstractmethod, abstractproperty
from argparse import ArgumentParser, SUPPRESS
from distutils.util import strtobool
from itertools import chain
from urlparse import urlparse
import logging
import json
import os
import tempfile

from mozdevice import DroidADB
from mozprofile import DEFAULT_PORTS
import mozinfo
import mozlog
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


# Maps test flavors to data needed to run them
ALL_FLAVORS = {
    'mochitest': {
        'suite': 'plain',
        'aliases': ('plain', 'mochitest'),
        'enabled_apps': ('firefox', 'android'),
        'extra_args': {
            'flavor': 'plain',
        },
        'install_subdir': 'tests',
    },
    'chrome': {
        'suite': 'chrome',
        'aliases': ('chrome', 'mochitest-chrome'),
        'enabled_apps': ('firefox', 'android'),
        'extra_args': {
            'flavor': 'chrome',
        }
    },
    'browser-chrome': {
        'suite': 'browser',
        'aliases': ('browser', 'browser-chrome', 'mochitest-browser-chrome', 'bc'),
        'enabled_apps': ('firefox',),
        'extra_args': {
            'flavor': 'browser',
        }
    },
    'a11y': {
        'suite': 'a11y',
        'aliases': ('a11y', 'mochitest-a11y', 'accessibility'),
        'enabled_apps': ('firefox',),
        'extra_args': {
            'flavor': 'a11y',
        }
    },
}
SUPPORTED_FLAVORS = list(chain.from_iterable([f['aliases'] for f in ALL_FLAVORS.values()]))
CANONICAL_FLAVORS = sorted([f['aliases'][0] for f in ALL_FLAVORS.values()])


def get_default_valgrind_suppression_files():
    # We are trying to locate files in the source tree.  So if we
    # don't know where the source tree is, we must give up.
    #
    # When this is being run by |mach mochitest --valgrind ...|, it is
    # expected that |build_obj| is not None, and so the logic below will
    # select the correct suppression files.
    #
    # When this is run from mozharness, |build_obj| is None, and we expect
    # that testing/mozharness/configs/unittests/linux_unittests.py will
    # select the correct suppression files (and paths to them) and
    # will specify them using the --valgrind-supp-files= flag.  Hence this
    # function will not get called when running from mozharness.
    #
    # Note: keep these Valgrind .sup file names consistent with those
    # in testing/mozharness/configs/unittests/linux_unittest.py.
    if build_obj is None or build_obj.topsrcdir is None:
        return []

    supps_path = os.path.join(build_obj.topsrcdir, "build", "valgrind")

    rv = []
    if mozinfo.os == "linux":
        if mozinfo.processor == "x86_64":
            rv.append(os.path.join(supps_path, "x86_64-redhat-linux-gnu.sup"))
            rv.append(os.path.join(supps_path, "cross-architecture.sup"))
        elif mozinfo.processor == "x86":
            rv.append(os.path.join(supps_path, "i386-redhat-linux-gnu.sup"))
            rv.append(os.path.join(supps_path, "cross-architecture.sup"))

    return rv


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

    args = [
        [["test_paths"],
         {"nargs": "*",
          "metavar": "TEST",
          "default": [],
          "help": "Test to run. Can be a single test file or a directory of tests "
                  "(to run recursively). If omitted, the entire suite is run.",
          }],
        [["-f", "--flavor"],
         {"choices": SUPPORTED_FLAVORS,
          "metavar": "{{{}}}".format(', '.join(CANONICAL_FLAVORS)),
          "default": None,
          "help": "Only run tests of this flavor.",
          }],
        [["--keep-open"],
         {"nargs": "?",
          "type": strtobool,
          "const": "true",
          "default": None,
          "help": "Always keep the browser open after tests complete. Or always close the "
                  "browser with --keep-open=false",
          }],
        [["--appname"],
         {"dest": "app",
          "default": None,
          "help": "Override the default binary used to run tests with the path provided, e.g "
                  "/usr/bin/firefox. If you have run ./mach package beforehand, you can "
                  "specify 'dist' to run tests against the distribution bundle's binary.",
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
        [["--run-by-manifest"],
         {"action": "store_true",
          "dest": "runByManifest",
          "help": "Run each manifest in a single browser instance with a fresh profile.",
          "default": False,
          "suppress": True,
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
          "help": "One of {} to determine the level of console logging.".format(
                  ', '.join(LOG_LEVELS)),
          "suppress": True,
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
        [["--subsuite"],
         {"default": None,
          "help": "Subsuite of tests to run. Unlike tags, subsuites also remove tests from "
                  "the default set. Only one can be specified at once.",
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
        [["--extra-mozinfo-json"],
         {"dest": "extra_mozinfo_json",
          "default": None,
          "help": "Filter tests based on a given mozinfo file.",
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
        [["--dump-tests"],
         {"dest": "dump_tests",
          "default": None,
          "help": "Specify path to a filename to dump all the tests that will be run",
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
        [["--disable-e10s"],
         {"action": "store_false",
          "default": True,
          "dest": "e10s",
          "help": "Run tests with electrolysis preferences and test filtering disabled.",
          }],
        [["--store-chrome-manifest"],
         {"action": "store",
          "help": "Destination path to write a copy of any chrome manifest "
                  "written by the harness.",
          "default": None,
          "suppress": True,
          }],
        [["--jscov-dir-prefix"],
         {"action": "store",
          "help": "Directory to store per-test line coverage data as json "
                  "(browser-chrome only). To emit lcov formatted data, set "
                  "JS_CODE_COVERAGE_OUTPUT_DIR in the environment.",
          "default": None,
          "suppress": True,
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
          "help": "Dump a DMD log (and an accompanying about:memory log) after each test. "
                  "These will be dumped into your default temp directory, NOT the directory "
                  "specified by --dump-output-directory. The logs are numbered by test, and "
                  "each test will include output that indicates the DMD output filename.",
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
        [["--headless"],
         {"action": "store_true",
          "dest": "headless",
          "default": False,
          "help": "Run tests in headless mode.",
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
        [["--valgrind"],
         {"default": None,
          "help": "Valgrind binary to run tests with. Program name or path.",
          }],
        [["--valgrind-args"],
         {"dest": "valgrindArgs",
          "default": None,
          "help": "Comma-separated list of extra arguments to pass to Valgrind.",
          }],
        [["--valgrind-supp-files"],
         {"dest": "valgrindSuppFiles",
          "default": None,
          "help": "Comma-separated list of suppression files to pass to Valgrind.",
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
        [["--marionette"],
         {"default": None,
          "help": "host:port to use when connecting to Marionette",
          }],
        [["--marionette-socket-timeout"],
         {"default": None,
          "help": "Timeout while waiting to receive a message from the marionette server.",
          "suppress": True,
          }],
        [["--marionette-startup-timeout"],
         {"default": None,
          "help": "Timeout while waiting for marionette server startup.",
          "suppress": True,
          }],
        [["--cleanup-crashes"],
         {"action": "store_true",
          "dest": "cleanupCrashes",
          "default": False,
          "help": "Delete pending crash reports before running tests.",
          "suppress": True,
          }],
        [["--websocket-process-bridge-port"],
         {"default": "8191",
          "dest": "websocket_process_bridge_port",
          "help": "Port for websocket/process bridge. Default 8191.",
          }],
        [["--failure-pattern-file"],
         {"default": None,
          "dest": "failure_pattern_file",
          "help": "File describes all failure patterns of the tests.",
          "suppress": True,
          }],
        [["--sandbox-read-whitelist"],
         {"default": [],
          "dest": "sandboxReadWhitelist",
          "action": "append",
          "help": "Path to add to the sandbox whitelist.",
          "suppress": True,
          }],
        [["--verify"],
         {"action": "store_true",
          "default": False,
          "help": "Run tests in verification mode: Run many times in different "
                  "ways, to see if there are intermittent failures.",
          }],
        [["--verify-max-time"],
         {"type": int,
          "default": 3600,
          "help": "Maximum time, in seconds, to run in --verify mode.",
          }],
    ]

    defaults = {
        # Bug 1065098 - The geckomediaplugin process fails to produce a leak
        # log for some reason.
        'ignoreMissingLeaks': ["geckomediaplugin"],
        'extensionsToExclude': ['specialpowers'],
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
        mozinfo.update({"nested_oop": options.nested_oop})

        # and android doesn't use 'app' the same way, so skip validation
        if parser.app != 'android':
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

        if options.flavor is None:
            options.flavor = 'plain'

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

        if options.extra_mozinfo_json:
            if not os.path.isfile(options.extra_mozinfo_json):
                parser.error("Error: couldn't find mozinfo.json at '%s'."
                             % options.extra_mozinfo_json)

            options.extra_mozinfo_json = json.load(open(options.extra_mozinfo_json))

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
        if options.xrePath:
            options.xrePath = self.get_full_path(options.xrePath, parser.oldcwd)

        if options.profilePath:
            options.profilePath = self.get_full_path(options.profilePath, parser.oldcwd)

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

        if options.jsdebugger:
            options.extraPrefs += [
                "devtools.debugger.remote-enabled=true",
                "devtools.chrome.enabled=true",
                "devtools.debugger.prompt-connection=false"
            ]

        if options.debugOnFailure and not options.jsdebugger:
            parser.error(
                "--debug-on-failure requires --jsdebugger.")

        if options.debuggerArgs and not options.debugger:
            parser.error(
                "--debugger-args requires --debugger.")

        if options.valgrind or options.debugger:
            # valgrind and some debuggers may cause Gecko to start slowly. Make sure
            # marionette waits long enough to connect.
            options.marionette_startup_timeout = 900
            options.marionette_socket_timeout = 540

        if options.store_chrome_manifest:
            options.store_chrome_manifest = os.path.abspath(options.store_chrome_manifest)
            if not os.path.isdir(os.path.dirname(options.store_chrome_manifest)):
                parser.error(
                    "directory for %s does not exist as a destination to copy a "
                    "chrome manifest." % options.store_chrome_manifest)

        if options.jscov_dir_prefix:
            options.jscov_dir_prefix = os.path.abspath(options.jscov_dir_prefix)
            if not os.path.isdir(options.jscov_dir_prefix):
                parser.error(
                    "directory %s does not exist as a destination for coverage "
                    "data." % options.jscov_dir_prefix)

        if options.testingModulesDir is None:
            # Try to guess the testing modules directory.
            possible = [os.path.join(here, os.path.pardir, 'modules')]
            if build_obj:
                possible.insert(0, os.path.join(build_obj.topobjdir, '_tests', 'modules'))

            for p in possible:
                if os.path.isdir(p):
                    options.testingModulesDir = p
                    break

        if build_obj:
            plugins_dir = os.path.join(build_obj.distdir, 'plugins')
            if os.path.isdir(plugins_dir) and plugins_dir not in options.extraProfileFiles:
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
            options.e10s = True

        options.leakThresholds = {
            "default": options.defaultLeakThreshold,
            "tab": options.defaultLeakThreshold,
            # GMP rarely gets a log, but when it does, it leaks a little.
            "geckomediaplugin": 20000,
        }

        # See the dependencies of bug 1401764.
        if mozinfo.isWin:
            options.leakThresholds["tab"] = 1000

        # XXX We can't normalize test_paths in the non build_obj case here,
        # because testRoot depends on the flavor, which is determined by the
        # mach command and therefore not finalized yet. Conversely, test paths
        # need to be normalized here for the mach case.
        if options.test_paths and build_obj:
            # Normalize test paths so they are relative to test root
            options.test_paths = [build_obj._wrap_path_argument(p).relpath()
                                  for p in options.test_paths]

        return options


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
        [["--adbpath"],
         {"dest": "adbPath",
          "default": None,
          "help": "Path to adb binary.",
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
        # we don't want to exclude specialpowers on android just yet
        'extensionsToExclude': [],
        # mochijar doesn't get installed via marionette on android
        'extensionsToInstall': [os.path.join(here, 'mochijar')],
        'logFile': 'mochitest.log',
        'utilityPath': None,
    }

    def validate(self, parser, options, context):
        """Validate android options."""

        if build_obj:
            options.log_mach = '-'

        device_args = {'deviceRoot': options.remoteTestRoot}
        device_args['adbPath'] = options.adbPath
        if options.deviceIP:
            device_args['host'] = options.deviceIP
            device_args['port'] = options.devicePort
        elif options.deviceSerial:
            device_args['deviceSerial'] = options.deviceSerial

        if options.log_tbpl_level == 'debug' or options.log_mach_level == 'debug':
            device_args['logLevel'] = logging.DEBUG
        options.dm = DroidADB(**device_args)

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

        if build_obj:
            options.topsrcdir = build_obj.topsrcdir

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
                if build_obj.substs.get('MOZ_BUILD_MOBILE_ANDROID_WITH_GRADLE'):
                    options.robocopApk = os.path.join(build_obj.topobjdir, 'gradle', 'build',
                                                      'mobile', 'android', 'app', 'outputs', 'apk',
                                                      'app-official-photon-debug-androidTest.apk')
                else:
                    options.robocopApk = os.path.join(build_obj.topobjdir, 'mobile', 'android',
                                                      'tests', 'browser',
                                                      'robocop', 'robocop-debug.apk')

        if options.robocopApk != "":
            if not os.path.exists(options.robocopApk):
                parser.error(
                    "Unable to find robocop APK '%s'" %
                    options.robocopApk)
            options.robocopApk = os.path.abspath(options.robocopApk)

        # Disable e10s by default on Android because we don't run Android
        # e10s jobs anywhere yet.
        options.e10s = False
        mozinfo.update({'e10s': options.e10s})

        # allow us to keep original application around for cleanup while
        # running robocop via 'am'
        options.remoteappname = options.app
        return options


container_map = {
    'generic': [MochitestArguments],
    'android': [MochitestArguments, AndroidArguments],
}


class MochitestArgumentParser(ArgumentParser):
    """%(prog)s [options] [test paths]"""

    _containers = None
    context = {}

    def __init__(self, app=None, **kwargs):
        ArgumentParser.__init__(self, usage=self.__doc__, conflict_handler='resolve', **kwargs)

        self.oldcwd = os.getcwd()
        self.app = app
        if not self.app and build_obj:
            if conditions.is_android(build_obj):
                self.app = 'android'
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
        mozlog.commandline.add_logging_group(self)

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

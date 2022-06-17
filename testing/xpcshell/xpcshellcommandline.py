# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import argparse

from mozlog import commandline


def add_common_arguments(parser):
    parser.add_argument(
        "--app-path",
        type=str,
        dest="appPath",
        default=None,
        help="application directory (as opposed to XRE directory)",
    )
    parser.add_argument(
        "--interactive",
        action="store_true",
        dest="interactive",
        default=False,
        help="don't automatically run tests, drop to an xpcshell prompt",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        dest="verbose",
        default=False,
        help="always print stdout and stderr from tests",
    )
    parser.add_argument(
        "--verbose-if-fails",
        action="store_true",
        dest="verboseIfFails",
        default=False,
        help="Output the log if a test fails, even when run in parallel",
    )
    parser.add_argument(
        "--keep-going",
        action="store_true",
        dest="keepGoing",
        default=False,
        help="continue running tests after test killed with control-C (SIGINT)",
    )
    parser.add_argument(
        "--logfiles",
        action="store_true",
        dest="logfiles",
        default=True,
        help="create log files (default, only used to override --no-logfiles)",
    )
    parser.add_argument(
        "--dump-tests",
        type=str,
        dest="dump_tests",
        default=None,
        help="Specify path to a filename to dump all the tests that will be run",
    )
    parser.add_argument(
        "--manifest",
        type=str,
        dest="manifest",
        default=None,
        help="Manifest of test directories to use",
    )
    parser.add_argument(
        "--no-logfiles",
        action="store_false",
        dest="logfiles",
        help="don't create log files",
    )
    parser.add_argument(
        "--sequential",
        action="store_true",
        dest="sequential",
        default=False,
        help="Run all tests sequentially",
    )
    parser.add_argument(
        "--temp-dir",
        dest="tempDir",
        default=None,
        help="Directory to use for temporary files",
    )
    parser.add_argument(
        "--testing-modules-dir",
        dest="testingModulesDir",
        default=None,
        help="Directory where testing modules are located.",
    )
    parser.add_argument(
        "--test-plugin-path",
        type=str,
        dest="pluginsPath",
        default=None,
        help="Path to the location of a plugins directory containing the "
        "test plugin or plugins required for tests. "
        "By default xpcshell's dir svc provider returns gre/plugins. "
        "Use test-plugin-path to add a directory "
        "to return for NS_APP_PLUGINS_DIR_LIST when queried.",
    )
    parser.add_argument(
        "--total-chunks",
        type=int,
        dest="totalChunks",
        default=1,
        help="how many chunks to split the tests up into",
    )
    parser.add_argument(
        "--this-chunk",
        type=int,
        dest="thisChunk",
        default=1,
        help="which chunk to run between 1 and --total-chunks",
    )
    parser.add_argument(
        "--profile-name",
        type=str,
        dest="profileName",
        default=None,
        help="name of application profile being tested",
    )
    parser.add_argument(
        "--build-info-json",
        type=str,
        dest="mozInfo",
        default=None,
        help="path to a mozinfo.json including information about the build "
        "configuration. defaults to looking for mozinfo.json next to "
        "the script.",
    )
    parser.add_argument(
        "--shuffle",
        action="store_true",
        dest="shuffle",
        default=False,
        help="Execute tests in random order",
    )
    parser.add_argument(
        "--xre-path",
        action="store",
        type=str,
        dest="xrePath",
        # individual scripts will set a sane default
        default=None,
        help="absolute path to directory containing XRE (probably xulrunner)",
    )
    parser.add_argument(
        "--symbols-path",
        action="store",
        type=str,
        dest="symbolsPath",
        default=None,
        help="absolute path to directory containing breakpad symbols, "
        "or the URL of a zip file containing symbols",
    )
    parser.add_argument(
        "--jscov-dir-prefix",
        action="store",
        type=str,
        dest="jscovdir",
        default=argparse.SUPPRESS,
        help="Directory to store per-test javascript line coverage data as json.",
    )
    parser.add_argument(
        "--debugger",
        action="store",
        dest="debugger",
        help="use the given debugger to launch the application",
    )
    parser.add_argument(
        "--debugger-args",
        action="store",
        dest="debuggerArgs",
        help="pass the given args to the debugger _before_ "
        "the application on the command line",
    )
    parser.add_argument(
        "--debugger-interactive",
        action="store_true",
        dest="debuggerInteractive",
        help="prevents the test harness from redirecting "
        "stdout and stderr for interactive debuggers",
    )
    parser.add_argument(
        "--jsdebugger",
        dest="jsDebugger",
        action="store_true",
        help="Waits for a devtools JS debugger to connect before " "starting the test.",
    )
    parser.add_argument(
        "--jsdebugger-port",
        type=int,
        dest="jsDebuggerPort",
        default=6000,
        help="The port to listen on for a debugger connection if "
        "--jsdebugger is specified.",
    )
    parser.add_argument(
        "--tag",
        action="append",
        dest="test_tags",
        default=None,
        help="filter out tests that don't have the given tag. Can be "
        "used multiple times in which case the test must contain "
        "at least one of the given tags.",
    )
    parser.add_argument(
        "--utility-path",
        action="store",
        dest="utility_path",
        default=None,
        help="Path to a directory containing utility programs, such "
        "as stack fixer scripts.",
    )
    parser.add_argument(
        "--xpcshell",
        action="store",
        dest="xpcshell",
        default=None,
        help="Path to xpcshell binary",
    )
    parser.add_argument(
        "--http3server",
        action="store",
        dest="http3server",
        default=None,
        help="Path to http3server binary",
    )
    # This argument can be just present, or the path to a manifest file. The
    # just-present case is usually used for mach which can provide a default
    # path to the failure file from the previous run
    parser.add_argument(
        "--rerun-failures",
        action="store_true",
        help="Rerun failures from the previous run, if any",
    )
    parser.add_argument(
        "--failure-manifest",
        action="store",
        help="Path to a manifest file from which to rerun failures "
        "(with --rerun-failure) or in which to record failed tests",
    )
    parser.add_argument(
        "--threads",
        type=int,
        dest="threadCount",
        default=0,
        help="override the number of jobs (threads) when running tests "
        "in parallel, the default is CPU x 1.5 when running via mach "
        "and CPU x 4 when running in automation",
    )
    parser.add_argument(
        "--setpref",
        action="append",
        dest="extraPrefs",
        metavar="PREF=VALUE",
        help="Defines an extra user preference (can be passed multiple times.",
    )
    parser.add_argument(
        "testPaths", nargs="*", default=None, help="Paths of tests to run."
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        default=False,
        help="Run tests in verification mode: Run many times in different "
        "ways, to see if there are intermittent failures.",
    )
    parser.add_argument(
        "--verify-max-time",
        dest="verifyMaxTime",
        type=int,
        default=3600,
        help="Maximum time, in seconds, to run in --verify mode.",
    )
    parser.add_argument(
        "--headless",
        action="store_true",
        default=False,
        dest="headless",
        help="Enable headless mode by default for tests which don't specify "
        "whether to use headless mode",
    )
    parser.add_argument(
        "--conditioned-profile",
        action="store_true",
        default=False,
        dest="conditionedProfile",
        help="Run with conditioned profile instead of fresh blank profile",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        default=False,
        dest="self_test",
        help="Run self tests",
    )
    parser.add_argument(
        "--run-failures",
        action="store",
        default="",
        dest="runFailures",
        help="Run failures matching keyword",
    )
    parser.add_argument(
        "--timeout-as-pass",
        action="store_true",
        default=False,
        dest="timeoutAsPass",
        help="Harness level timeouts will be treated as passing",
    )
    parser.add_argument(
        "--crash-as-pass",
        action="store_true",
        default=False,
        dest="crashAsPass",
        help="Harness level crashes will be treated as passing",
    )
    parser.add_argument(
        "--disable-fission",
        action="store_true",
        default=False,
        dest="disableFission",
        help="disable fission mode (back to e10s || 1proc)",
    )


def add_remote_arguments(parser):
    parser.add_argument(
        "--objdir",
        action="store",
        type=str,
        dest="objdir",
        help="Local objdir, containing xpcshell binaries.",
    )

    parser.add_argument(
        "--apk",
        action="store",
        type=str,
        dest="localAPK",
        help="Local path to Firefox for Android APK.",
    )

    parser.add_argument(
        "--deviceSerial",
        action="store",
        type=str,
        dest="deviceSerial",
        help="adb serial number of remote device. This is required "
        "when more than one device is connected to the host. "
        "Use 'adb devices' to see connected devices.",
    )

    parser.add_argument(
        "--adbPath",
        action="store",
        type=str,
        dest="adbPath",
        default=None,
        help="Path to adb binary.",
    )

    parser.add_argument(
        "--noSetup",
        action="store_false",
        dest="setup",
        default=True,
        help="Do not copy any files to device (to be used only if "
        "device is already setup).",
    )
    parser.add_argument(
        "--no-install",
        action="store_false",
        dest="setup",
        default=True,
        help="Don't install the app or any files to the device (to be used if "
        "the device is already set up)",
    )

    parser.add_argument(
        "--local-bin-dir",
        action="store",
        type=str,
        dest="localBin",
        help="Local path to bin directory.",
    )

    parser.add_argument(
        "--remoteTestRoot",
        action="store",
        type=str,
        dest="remoteTestRoot",
        help="Remote directory to use as test root " "(eg. /data/local/tmp/test_root).",
    )


def parser_desktop():
    parser = argparse.ArgumentParser()
    add_common_arguments(parser)
    commandline.add_logging_group(parser)

    return parser


def parser_remote():
    parser = argparse.ArgumentParser()
    common = parser.add_argument_group("Common Options")
    add_common_arguments(common)
    remote = parser.add_argument_group("Remote Options")
    add_remote_arguments(remote)
    commandline.add_logging_group(parser)

    return parser

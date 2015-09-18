import argparse

def add_common_arguments(parser):
    parser.add_argument("--app-path",
                        type=unicode, dest="appPath", default=None,
                        help="application directory (as opposed to XRE directory)")
    parser.add_argument("--interactive",
                        action="store_true", dest="interactive", default=False,
                        help="don't automatically run tests, drop to an xpcshell prompt")
    parser.add_argument("--verbose",
                        action="store_true", dest="verbose", default=False,
                        help="always print stdout and stderr from tests")
    parser.add_argument("--keep-going",
                        action="store_true", dest="keepGoing", default=False,
                        help="continue running tests after test killed with control-C (SIGINT)")
    parser.add_argument("--logfiles",
                        action="store_true", dest="logfiles", default=True,
                        help="create log files (default, only used to override --no-logfiles)")
    parser.add_argument("--dump-tests", type=str, dest="dump_tests", default=None,
                        help="Specify path to a filename to dump all the tests that will be run")
    parser.add_argument("--manifest",
                        type=unicode, dest="manifest", default=None,
                        help="Manifest of test directories to use")
    parser.add_argument("--no-logfiles",
                        action="store_false", dest="logfiles",
                        help="don't create log files")
    parser.add_argument("--sequential",
                        action="store_true", dest="sequential", default=False,
                        help="Run all tests sequentially")
    parser.add_argument("--testing-modules-dir",
                        dest="testingModulesDir", default=None,
                        help="Directory where testing modules are located.")
    parser.add_argument("--test-plugin-path",
                        type=str, dest="pluginsPath", default=None,
                        help="Path to the location of a plugins directory containing the test plugin or plugins required for tests. "
                        "By default xpcshell's dir svc provider returns gre/plugins. Use test-plugin-path to add a directory "
                        "to return for NS_APP_PLUGINS_DIR_LIST when queried.")
    parser.add_argument("--total-chunks",
                        type=int, dest="totalChunks", default=1,
                        help="how many chunks to split the tests up into")
    parser.add_argument("--this-chunk",
                        type=int, dest="thisChunk", default=1,
                        help="which chunk to run between 1 and --total-chunks")
    parser.add_argument("--profile-name",
                        type=str, dest="profileName", default=None,
                        help="name of application profile being tested")
    parser.add_argument("--build-info-json",
                        type=str, dest="mozInfo", default=None,
                        help="path to a mozinfo.json including information about the build configuration. defaults to looking for mozinfo.json next to the script.")
    parser.add_argument("--shuffle",
                        action="store_true", dest="shuffle", default=False,
                        help="Execute tests in random order")
    parser.add_argument("--xre-path",
                        action="store", type=str, dest="xrePath",
                        # individual scripts will set a sane default
                        default=None,
                        help="absolute path to directory containing XRE (probably xulrunner)")
    parser.add_argument("--symbols-path",
                        action="store", type=str, dest="symbolsPath",
                        default=None,
                        help="absolute path to directory containing breakpad symbols, or the URL of a zip file containing symbols")
    parser.add_argument("--debugger",
                        action="store", dest="debugger",
                        help="use the given debugger to launch the application")
    parser.add_argument("--debugger-args",
                        action="store", dest="debuggerArgs",
                        help="pass the given args to the debugger _before_ "
                        "the application on the command line")
    parser.add_argument("--debugger-interactive",
                        action="store_true", dest="debuggerInteractive",
                        help="prevents the test harness from redirecting "
                        "stdout and stderr for interactive debuggers")
    parser.add_argument("--jsdebugger", dest="jsDebugger", action="store_true",
                        help="Waits for a devtools JS debugger to connect before "
                        "starting the test.")
    parser.add_argument("--jsdebugger-port", type=int, dest="jsDebuggerPort",
                        default=6000,
                        help="The port to listen on for a debugger connection if "
                        "--jsdebugger is specified.")
    parser.add_argument("--tag",
                        action="append", dest="test_tags",
                        default=None,
                        help="filter out tests that don't have the given tag. Can be "
                        "used multiple times in which case the test must contain "
                        "at least one of the given tags.")
    parser.add_argument("--utility-path",
                        action="store", dest="utility_path",
                        default=None,
                        help="Path to a directory containing utility programs, such "
                        "as stack fixer scripts.")
    parser.add_argument("--xpcshell",
                        action="store", dest="xpcshell",
                        default=None,
                        help="Path to xpcshell binary")
    # This argument can be just present, or the path to a manifest file. The
    # just-present case is usually used for mach which can provide a default
    # path to the failure file from the previous run
    parser.add_argument("--rerun-failures",
                        action="store_true",
                        help="Rerun failures from the previous run, if any")
    parser.add_argument("--failure-manifest",
                        action="store",
                        help="Path to a manifest file from which to rerun failures "
                        "(with --rerun-failure) or in which to record failed tests")
    parser.add_argument("testPaths", nargs="*", default=None,
                        help="Paths of tests to run.")

def add_remote_arguments(parser):
    parser.add_argument("--deviceIP", action="store", type=str, dest="deviceIP",
                        help="ip address of remote device to test")

    parser.add_argument("--devicePort", action="store", type=str, dest="devicePort",
                        default=20701, help="port of remote device to test")

    parser.add_argument("--dm_trans", action="store", type=str, dest="dm_trans",
                        choices=["adb", "sut"], default="adb",
                        help="the transport to use to communicate with device: [adb|sut]; default=adb")

    parser.add_argument("--objdir", action="store", type=str, dest="objdir",
                        help="local objdir, containing xpcshell binaries")


    parser.add_argument("--apk", action="store", type=str, dest="localAPK",
                        help="local path to Fennec APK")


    parser.add_argument("--noSetup", action="store_false", dest="setup", default=True,
                        help="do not copy any files to device (to be used only if device is already setup)")

    parser.add_argument("--local-lib-dir", action="store", type=str, dest="localLib",
                        help="local path to library directory")

    parser.add_argument("--local-bin-dir", action="store", type=str, dest="localBin",
                        help="local path to bin directory")

    parser.add_argument("--remoteTestRoot", action="store", type=str, dest="remoteTestRoot",
                        help="remote directory to use as test root (eg. /mnt/sdcard/tests or /data/local/tests)")

def add_b2g_arguments(parser):
    parser.add_argument('--b2gpath', action='store', type=str, dest='b2g_path',
                        help="Path to B2G repo or qemu dir")

    parser.add_argument('--emupath', action='store', type=str, dest='emu_path',
                        help="Path to emulator folder (if different "
                        "from b2gpath")

    parser.add_argument('--no-clean', action='store_false', dest='clean', default=True,
                        help="Do not clean TESTROOT. Saves [lots of] time")

    parser.add_argument('--emulator', action='store', type=str, dest='emulator',
                        default="arm", choices=["x86", "arm"],
                        help="Architecture of emulator to use: x86 or arm")

    parser.add_argument('--no-window', action='store_true', dest='no_window', default=False,
                        help="Pass --no-window to the emulator")

    parser.add_argument('--adbpath', action='store', type=str, dest='adb_path',
                        default="adb", help="Path to adb")

    parser.add_argument('--address', action='store', type=str, dest='address',
                        help="host:port of running Gecko instance to connect to")

    parser.add_argument('--use-device-libs', action='store_true', dest='use_device_libs',
                        default=None, help="Don't push .so's")

    parser.add_argument("--gecko-path", action="store", type=str, dest="geckoPath",
                        help="the path to a gecko distribution that should "
                        "be installed on the emulator prior to test")

    parser.add_argument("--logdir", action="store", type=str, dest="logdir",
                        help="directory to store log files")

    parser.add_argument('--busybox', action='store', type=str, dest='busybox',
                        help="Path to busybox binary to install on device")

    parser.set_defaults(remoteTestRoot="/data/local/tests",
                        dm_trans="adb")

def parser_desktop():
    parser = argparse.ArgumentParser()
    add_common_arguments(parser)
    return parser

def parser_remote():
    parser = argparse.ArgumentParser()
    common = parser.add_argument_group("Common Options")
    add_common_arguments(common)
    remote = parser.add_argument_group("Remote Options")
    add_remote_arguments(remote)

    return parser

def parser_b2g():
    parser = argparse.ArgumentParser()
    common = parser.add_argument_group("Common Options")
    add_common_arguments(common)
    remote = parser.add_argument_group("Remote Options")
    add_remote_arguments(remote)
    b2g = parser.add_argument_group("B2G Options")
    add_b2g_arguments(b2g)

    return parser

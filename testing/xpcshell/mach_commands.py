# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Integrates the xpcshell test runner with mach.

from __future__ import unicode_literals, print_function

import mozpack.path
import logging
import os
import shutil
import sys
import urllib2

from StringIO import StringIO

from mozbuild.base import (
    MachCommandBase,
    MozbuildObject,
    MachCommandConditions as conditions,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

ADB_NOT_FOUND = '''
The %s command requires the adb binary to be on your path.

If you have a B2G build, this can be found in
'%s/out/host/<platform>/bin'.
'''.lstrip()

BUSYBOX_URL = 'http://www.busybox.net/downloads/binaries/latest/busybox-armv7l'


if sys.version_info[0] < 3:
    unicode_type = unicode
else:
    unicode_type = str

# Simple filter to omit the message emitted as a test file begins.
class TestStartFilter(logging.Filter):
    def filter(self, record):
        return not record.params['msg'].endswith("running test ...")

# This should probably be consolidated with similar classes in other test
# runners.
class InvalidTestPathError(Exception):
    """Exception raised when the test path is not valid."""


class XPCShellRunner(MozbuildObject):
    """Run xpcshell tests."""
    def run_suite(self, **kwargs):
        from manifestparser import TestManifest
        manifest = TestManifest(manifests=[os.path.join(self.topobjdir,
            '_tests', 'xpcshell', 'xpcshell.ini')])

        return self._run_xpcshell_harness(manifest=manifest, **kwargs)

    def run_test(self, test_file, interactive=False,
                 keep_going=False, sequential=False, shuffle=False,
                 debugger=None, debuggerArgs=None, debuggerInteractive=None,
                 rerun_failures=False,
                 # ignore parameters from other platforms' options
                 **kwargs):
        """Runs an individual xpcshell test."""
        from mozbuild.testing import TestResolver
        from manifestparser import TestManifest

        # TODO Bug 794506 remove once mach integrates with virtualenv.
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        if test_file == 'all':
            self.run_suite(interactive=interactive,
                           keep_going=keep_going, shuffle=shuffle, sequential=sequential,
                           debugger=debugger, debuggerArgs=debuggerArgs,
                           debuggerInteractive=debuggerInteractive,
                           rerun_failures=rerun_failures)
            return

        resolver = self._spawn(TestResolver)
        tests = list(resolver.resolve_tests(path=test_file, flavor='xpcshell',
            cwd=self.cwd))

        if not tests:
            raise InvalidTestPathError('We could not find an xpcshell test '
                'for the passed test path. Please select a path that is '
                'a test file or is a directory containing xpcshell tests.')

        # Dynamically write out a manifest holding all the discovered tests.
        manifest = TestManifest()
        manifest.tests.extend(tests)

        args = {
            'interactive': interactive,
            'keep_going': keep_going,
            'shuffle': shuffle,
            'sequential': sequential,
            'debugger': debugger,
            'debuggerArgs': debuggerArgs,
            'debuggerInteractive': debuggerInteractive,
            'rerun_failures': rerun_failures,
            'manifest': manifest,
        }

        return self._run_xpcshell_harness(**args)

    def _run_xpcshell_harness(self, manifest,
                              test_path=None, shuffle=False, interactive=False,
                              keep_going=False, sequential=False,
                              debugger=None, debuggerArgs=None, debuggerInteractive=None,
                              rerun_failures=False):

        # Obtain a reference to the xpcshell test runner.
        import runxpcshelltests

        dummy_log = StringIO()
        xpcshell = runxpcshelltests.XPCShellTests(log=dummy_log)
        self.log_manager.enable_unstructured()

        xpcshell_filter = TestStartFilter()
        self.log_manager.terminal_handler.addFilter(xpcshell_filter)

        tests_dir = os.path.join(self.topobjdir, '_tests', 'xpcshell')
        modules_dir = os.path.join(self.topobjdir, '_tests', 'modules')
        # We want output from the test to be written immediately if we are only
        # running a single test.
        verbose_output = test_path is not None or (manifest and len(manifest.test_paths())==1)

        args = {
            'manifest': manifest,
            'xpcshell': os.path.join(self.bindir, 'xpcshell'),
            'mozInfo': os.path.join(self.topobjdir, 'mozinfo.json'),
            'symbolsPath': os.path.join(self.distdir, 'crashreporter-symbols'),
            'interactive': interactive,
            'keepGoing': keep_going,
            'logfiles': False,
            'sequential': sequential,
            'shuffle': shuffle,
            'testsRootDir': tests_dir,
            'testingModulesDir': modules_dir,
            'profileName': 'firefox',
            'verbose': test_path is not None,
            'xunitFilename': os.path.join(self.statedir, 'xpchsell.xunit.xml'),
            'xunitName': 'xpcshell',
            'pluginsPath': os.path.join(self.distdir, 'plugins'),
            'debugger': debugger,
            'debuggerArgs': debuggerArgs,
            'debuggerInteractive': debuggerInteractive,
            'on_message': (lambda obj, msg: xpcshell.log.info(msg.decode('utf-8', 'replace'))) \
                            if verbose_output else None,
        }

        if test_path is not None:
            args['testPath'] = test_path

        # A failure manifest is written by default. If --rerun-failures is
        # specified and a prior failure manifest is found, the prior manifest
        # will be run. A new failure manifest is always written over any
        # prior failure manifest.
        failure_manifest_path = os.path.join(self.statedir, 'xpcshell.failures.ini')
        rerun_manifest_path = os.path.join(self.statedir, 'xpcshell.rerun.ini')
        if os.path.exists(failure_manifest_path) and rerun_failures:
            shutil.move(failure_manifest_path, rerun_manifest_path)
            args['manifest'] = rerun_manifest_path
        elif os.path.exists(failure_manifest_path):
            os.remove(failure_manifest_path)
        elif rerun_failures:
            print("No failures were found to re-run.")
            return 0
        args['failureManifest'] = failure_manifest_path

        # Python through 2.7.2 has issues with unicode in some of the
        # arguments. Work around that.
        filtered_args = {}
        for k, v in args.items():
            if isinstance(v, unicode_type):
                v = v.encode('utf-8')

            if isinstance(k, unicode_type):
                k = k.encode('utf-8')

            filtered_args[k] = v

        result = xpcshell.runTests(**filtered_args)

        self.log_manager.terminal_handler.removeFilter(xpcshell_filter)
        self.log_manager.disable_unstructured()

        if not result and not xpcshell.sequential:
            print("Tests were run in parallel. Try running with --sequential "
                  "to make sure the failures were not caused by this.")
        return int(not result)

class AndroidXPCShellRunner(MozbuildObject):
    """Get specified DeviceManager"""
    def get_devicemanager(self, devicemanager, ip, port, remote_test_root):
        from mozdevice import devicemanagerADB, devicemanagerSUT
        dm = None
        if devicemanager == "adb":
            if ip:
                dm = devicemanagerADB.DeviceManagerADB(ip, port, packageName=None, deviceRoot=remote_test_root)
            else:
                dm = devicemanagerADB.DeviceManagerADB(packageName=None, deviceRoot=remote_test_root)
        else:
            if ip:
                dm = devicemanagerSUT.DeviceManagerSUT(ip, port, deviceRoot=remote_test_root)
            else:
                raise Exception("You must provide a device IP to connect to via the --ip option")
        return dm

    """Run Android xpcshell tests."""
    def run_test(self,
                 test_file, keep_going,
                 devicemanager, ip, port, remote_test_root, no_setup, local_apk,
                 # ignore parameters from other platforms' options
                 **kwargs):
        # TODO Bug 794506 remove once mach integrates with virtualenv.
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        import remotexpcshelltests

        dm = self.get_devicemanager(devicemanager, ip, port, remote_test_root)

        options = remotexpcshelltests.RemoteXPCShellOptions()
        options.shuffle = False
        options.sequential = True
        options.interactive = False
        options.debugger = None
        options.debuggerArgs = None
        options.setup = not no_setup
        options.keepGoing = keep_going
        options.objdir = self.topobjdir
        options.localLib = os.path.join(self.topobjdir, 'dist/fennec')
        options.localBin = os.path.join(self.topobjdir, 'dist/bin')
        options.testingModulesDir = os.path.join(self.topobjdir, '_tests/modules')
        options.mozInfo = os.path.join(self.topobjdir, 'mozinfo.json')
        options.manifest = os.path.join(self.topobjdir, '_tests/xpcshell/xpcshell_android.ini')
        options.symbolsPath = os.path.join(self.distdir, 'crashreporter-symbols')
        if local_apk:
            options.localAPK = local_apk
        else:
            for file in os.listdir(os.path.join(options.objdir, "dist")):
                if file.endswith(".apk") and file.startswith("fennec"):
                    options.localAPK = os.path.join(options.objdir, "dist")
                    options.localAPK = os.path.join(options.localAPK, file)
                    print ("using APK: " + options.localAPK)
                    break
            else:
                raise Exception("You must specify an APK")

        if test_file == 'all':
            testdirs = []
            options.testPath = None
            options.verbose = False
        else:
            testdirs = test_file
            options.testPath = test_file
            options.verbose = True
        dummy_log = StringIO()
        xpcshell = remotexpcshelltests.XPCShellRemote(dm, options, args=testdirs, log=dummy_log)
        self.log_manager.enable_unstructured()

        xpcshell_filter = TestStartFilter()
        self.log_manager.terminal_handler.addFilter(xpcshell_filter)

        result = xpcshell.runTests(xpcshell='xpcshell',
                      testClass=remotexpcshelltests.RemoteXPCShellTestThread,
                      testdirs=testdirs,
                      mobileArgs=xpcshell.mobileArgs,
                      **options.__dict__)

        self.log_manager.terminal_handler.removeFilter(xpcshell_filter)
        self.log_manager.disable_unstructured()

        return int(not result)

class B2GXPCShellRunner(MozbuildObject):
    def __init__(self, *args, **kwargs):
        MozbuildObject.__init__(self, *args, **kwargs)

        # TODO Bug 794506 remove once mach integrates with virtualenv.
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        build_path = os.path.join(self.topsrcdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        self.tests_dir = os.path.join(self.topobjdir, '_tests')
        self.xpcshell_dir = os.path.join(self.tests_dir, 'xpcshell')
        self.bin_dir = os.path.join(self.distdir, 'bin')

    def _download_busybox(self, b2g_home):
        system_bin = os.path.join(b2g_home, 'out', 'target', 'product', 'generic', 'system', 'bin')
        busybox_path = os.path.join(system_bin, 'busybox')

        if os.path.isfile(busybox_path):
            return busybox_path

        if not os.path.isdir(system_bin):
            os.makedirs(system_bin)

        try:
            data = urllib2.urlopen(BUSYBOX_URL)
        except urllib2.URLError:
            print('There was a problem downloading busybox. Proceeding without it,' \
                  'initial setup will be slow.')
            return

        with open(busybox_path, 'wb') as f:
            f.write(data.read())
        return busybox_path

    def run_test(self, test_file, b2g_home=None, busybox=None,
                 # ignore parameters from other platforms' options
                 **kwargs):
        try:
            import which
            which.which('adb')
        except which.WhichError:
            # TODO Find adb automatically if it isn't on the path
            print(ADB_NOT_FOUND % ('mochitest-remote', b2g_home))
            sys.exit(1)

        test_path = None
        if test_file:
            test_path = self._wrap_path_argument(test_file).relpath()

        import runtestsb2g
        parser = runtestsb2g.B2GOptions()
        options, args = parser.parse_args([])

        options.b2g_path = b2g_home
        options.busybox = busybox or os.environ.get('BUSYBOX')
        options.emulator = 'arm'
        options.localLib = self.bin_dir
        options.localBin = self.bin_dir
        options.logcat_dir = self.xpcshell_dir
        options.manifest = os.path.join(self.xpcshell_dir, 'xpcshell_b2g.ini')
        options.mozInfo = os.path.join(self.topobjdir, 'mozinfo.json')
        options.objdir = self.topobjdir
        options.symbolsPath = os.path.join(self.distdir, 'crashreporter-symbols'),
        options.testingModulesDir = os.path.join(self.tests_dir, 'modules')
        options.testsRootDir = self.xpcshell_dir
        options.testPath = test_path
        options.use_device_libs = True

        if not options.busybox:
            options.busybox = self._download_busybox(b2g_home)

        return runtestsb2g.run_remote_xpcshell(parser, options, args)

def is_platform_supported(cls):
    """Must have a Firefox, Android or B2G build."""
    return conditions.is_android(cls) or \
           conditions.is_b2g(cls) or \
           conditions.is_firefox(cls)

@CommandProvider
class MachCommands(MachCommandBase):
    def __init__(self, context):
        MachCommandBase.__init__(self, context)

        for attr in ('b2g_home', 'device_name'):
            setattr(self, attr, getattr(context, attr, None))

    @Command('xpcshell-test', category='testing',
        conditions=[is_platform_supported],
        description='Run XPCOM Shell tests.')
    @CommandArgument('test_file', default='all', nargs='?', metavar='TEST',
        help='Test to run. Can be specified as a single JS file, a directory, '
             'or omitted. If omitted, the entire test suite is executed.')
    @CommandArgument("--debugger", default=None, metavar='DEBUGGER',
                     help = "Run xpcshell under the given debugger.")
    @CommandArgument("--debugger-args", default=None, metavar='ARGS', type=str,
                     dest = "debuggerArgs",
                     help = "pass the given args to the debugger _before_ "
                            "the application on the command line")
    @CommandArgument("--debugger-interactive", action = "store_true",
                     dest = "debuggerInteractive",
                     help = "prevents the test harness from redirecting "
                            "stdout and stderr for interactive debuggers")
    @CommandArgument('--interactive', '-i', action='store_true',
        help='Open an xpcshell prompt before running tests.')
    @CommandArgument('--keep-going', '-k', action='store_true',
        help='Continue running tests after a SIGINT is received.')
    @CommandArgument('--sequential', action='store_true',
        help='Run the tests sequentially.')
    @CommandArgument('--shuffle', '-s', action='store_true',
        help='Randomize the execution order of tests.')
    @CommandArgument('--rerun-failures', action='store_true',
        help='Reruns failures from last time.')
    @CommandArgument('--devicemanager', default='adb', type=str,
        help='(Android) Type of devicemanager to use for communication: adb or sut')
    @CommandArgument('--ip', type=str, default=None,
        help='(Android) IP address of device')
    @CommandArgument('--port', type=int, default=20701,
        help='(Android) Port of device')
    @CommandArgument('--remote_test_root', type=str, default=None,
        help='(Android) Remote test root such as /mnt/sdcard or /data/local')
    @CommandArgument('--no-setup', action='store_true',
        help='(Android) Do not copy files to device')
    @CommandArgument('--local-apk', type=str, default=None,
        help='(Android) Use specified Fennec APK')
    @CommandArgument('--busybox', type=str, default=None,
        help='(B2G) Path to busybox binary (speeds up installation of tests).')
    def run_xpcshell_test(self, **params):
        from mozbuild.controller.building import BuildDriver

        # We should probably have a utility function to ensure the tree is
        # ready to run tests. Until then, we just create the state dir (in
        # case the tree wasn't built with mach).
        self._ensure_state_subdir_exists('.')

        driver = self._spawn(BuildDriver)
        driver.install_tests(remove=False)

        if conditions.is_android(self):
            xpcshell = self._spawn(AndroidXPCShellRunner)
        elif conditions.is_b2g(self):
            xpcshell = self._spawn(B2GXPCShellRunner)
            params['b2g_home'] = self.b2g_home
        else:
            xpcshell = self._spawn(XPCShellRunner)
        xpcshell.cwd = self._mach_context.cwd

        try:
            return xpcshell.run_test(**params)
        except InvalidTestPathError as e:
            print(e.message)
            return 1

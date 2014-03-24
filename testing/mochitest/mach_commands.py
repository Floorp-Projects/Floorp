# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import logging
import mozpack.path
import os
import platform
import sys
import warnings
import which

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
    MozbuildObject,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mach.logging import StructuredHumanFormatter

ADB_NOT_FOUND = '''
The %s command requires the adb binary to be on your path.

If you have a B2G build, this can be found in
'%s/out/host/<platform>/bin'.
'''.lstrip()

GAIA_PROFILE_NOT_FOUND = '''
The %s command requires a non-debug gaia profile. Either pass in --profile,
or set the GAIA_PROFILE environment variable.

If you do not have a non-debug gaia profile, you can build one:
    $ git clone https://github.com/mozilla-b2g/gaia
    $ cd gaia
    $ make

The profile should be generated in a directory called 'profile'.
'''.lstrip()

GAIA_PROFILE_IS_DEBUG = '''
The %s command requires a non-debug gaia profile. The specified profile,
%s, is a debug profile.

If you do not have a non-debug gaia profile, you can build one:
    $ git clone https://github.com/mozilla-b2g/gaia
    $ cd gaia
    $ make

The profile should be generated in a directory called 'profile'.
'''.lstrip()


class UnexpectedFilter(logging.Filter):
    def filter(self, record):
        msg = getattr(record, 'params', {}).get('msg', '')
        return 'TEST-UNEXPECTED-' in msg


class MochitestRunner(MozbuildObject):
    """Easily run mochitests.

    This currently contains just the basics for running mochitests. We may want
    to hook up result parsing, etc.
    """

    def get_webapp_runtime_path(self):
        import mozinfo
        appname = 'webapprt-stub' + mozinfo.info.get('bin_suffix', '')
        if sys.platform.startswith('darwin'):
            appname = os.path.join(self.distdir, self.substs['MOZ_MACBUNDLE_NAME'],
            'Contents', 'MacOS', appname)
        else:
            appname = os.path.join(self.distdir, 'bin', appname)
        return appname

    def __init__(self, *args, **kwargs):
        MozbuildObject.__init__(self, *args, **kwargs)

        # TODO Bug 794506 remove once mach integrates with virtualenv.
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        self.tests_dir = os.path.join(self.topobjdir, '_tests')
        self.mochitest_dir = os.path.join(self.tests_dir, 'testing', 'mochitest')
        self.bin_dir = os.path.join(self.topobjdir, 'dist', 'bin')

    def run_b2g_test(self, test_paths=None, b2g_home=None, xre_path=None,
                     total_chunks=None, this_chunk=None, no_window=None,
                     **kwargs):
        """Runs a b2g mochitest.

        test_paths is an enumerable of paths to tests. It can be a relative path
        from the top source directory, an absolute filename, or a directory
        containing test files.
        """
        # Need to call relpath before os.chdir() below.
        test_path = ''
        if test_paths:
            if len(test_paths) > 1:
                print('Warning: Only the first test path will be used.')
            test_path = self._wrap_path_argument(test_paths[0]).relpath()

        # TODO without os.chdir, chained imports fail below
        os.chdir(self.mochitest_dir)

        # The imp module can spew warnings if the modules below have
        # already been imported, ignore them.
        with warnings.catch_warnings():
            warnings.simplefilter('ignore')

            import imp
            path = os.path.join(self.mochitest_dir, 'runtestsb2g.py')
            with open(path, 'r') as fh:
                imp.load_module('mochitest', fh, path,
                    ('.py', 'r', imp.PY_SOURCE))

            import mochitest
            from mochitest_options import B2GOptions

        parser = B2GOptions()
        options = parser.parse_args([])[0]

        test_path_dir = False;
        if test_path:
            test_root_file = mozpack.path.join(self.mochitest_dir, 'tests', test_path)
            if not os.path.exists(test_root_file):
                print('Specified test path does not exist: %s' % test_root_file)
                return 1
            if os.path.isdir(test_root_file):
                test_path_dir = True;
            options.testPath = test_path

        for k, v in kwargs.iteritems():
            setattr(options, k, v)
        options.noWindow = no_window
        options.totalChunks = total_chunks
        options.thisChunk = this_chunk

        options.symbolsPath = os.path.join(self.distdir, 'crashreporter-symbols')

        options.consoleLevel = 'INFO'
        if conditions.is_b2g_desktop(self):

            options.profile = options.profile or os.environ.get('GAIA_PROFILE')
            if not options.profile:
                print(GAIA_PROFILE_NOT_FOUND % 'mochitest-b2g-desktop')
                return 1

            if os.path.isfile(os.path.join(options.profile, 'extensions', \
                    'httpd@gaiamobile.org')):
                print(GAIA_PROFILE_IS_DEBUG % ('mochitest-b2g-desktop',
                                               options.profile))
                return 1

            options.desktop = True
            options.app = self.get_binary_path()
            if not options.app.endswith('-bin'):
                options.app = '%s-bin' % options.app
            if not os.path.isfile(options.app):
                options.app = options.app[:-len('-bin')]

            return mochitest.run_desktop_mochitests(parser, options)

        try:
            which.which('adb')
        except which.WhichError:
            # TODO Find adb automatically if it isn't on the path
            print(ADB_NOT_FOUND % ('mochitest-remote', b2g_home))
            return 1

        options.b2gPath = b2g_home
        options.logcat_dir = self.mochitest_dir
        options.httpdPath = self.mochitest_dir
        options.xrePath = xre_path
        return mochitest.run_remote_mochitests(parser, options)

    def run_desktop_test(self, context, suite=None, test_paths=None, debugger=None,
        debugger_args=None, slowscript=False, shuffle=False, keep_open=False,
        rerun_failures=False, no_autorun=False, repeat=0, run_until_failure=False,
        slow=False, chunk_by_dir=0, total_chunks=None, this_chunk=None,
        jsdebugger=False, debug_on_failure=False, start_at=None, end_at=None,
        e10s=False, dmd=False, dump_output_directory=None,
        dump_about_memory_after_test=False, dump_dmd_after_test=False,
        install_extension=None, **kwargs):
        """Runs a mochitest.

        test_paths are path to tests. They can be a relative path from the
        top source directory, an absolute filename, or a directory containing
        test files.

        suite is the type of mochitest to run. It can be one of ('plain',
        'chrome', 'browser', 'metro', 'a11y').

        debugger is a program name or path to a binary (presumably a debugger)
        to run the test in. e.g. 'gdb'

        debugger_args are the arguments passed to the debugger.

        slowscript is true if the user has requested the SIGSEGV mechanism of
        invoking the slow script dialog.

        shuffle is whether test order should be shuffled (defaults to false).

        keep_open denotes whether to keep the browser open after tests
        complete.
        """
        if rerun_failures and test_paths:
            print('Cannot specify both --rerun-failures and a test path.')
            return 1

        # Need to call relpath before os.chdir() below.
        if test_paths:
            test_paths = [self._wrap_path_argument(p).relpath() for p in test_paths]

        failure_file_path = os.path.join(self.statedir, 'mochitest_failures.json')

        if rerun_failures and not os.path.exists(failure_file_path):
            print('No failure file present. Did you run mochitests before?')
            return 1

        from StringIO import StringIO

        # runtests.py is ambiguous, so we load the file/module manually.
        if 'mochitest' not in sys.modules:
            import imp
            path = os.path.join(self.mochitest_dir, 'runtests.py')
            with open(path, 'r') as fh:
                imp.load_module('mochitest', fh, path,
                    ('.py', 'r', imp.PY_SOURCE))

        import mozinfo
        import mochitest
        from manifestparser import TestManifest
        from mozbuild.testing import TestResolver

        # This is required to make other components happy. Sad, isn't it?
        os.chdir(self.topobjdir)

        # Automation installs its own stream handler to stdout. Since we want
        # all logging to go through us, we just remove their handler.
        remove_handlers = [l for l in logging.getLogger().handlers
            if isinstance(l, logging.StreamHandler)]
        for handler in remove_handlers:
            logging.getLogger().removeHandler(handler)

        runner = mochitest.Mochitest()

        opts = mochitest.MochitestOptions()
        options, args = opts.parse_args([])

        flavor = suite

        # Need to set the suite options before verifyOptions below.
        if suite == 'plain':
            # Don't need additional options for plain.
            flavor = 'mochitest'
        elif suite == 'chrome':
            options.chrome = True
        elif suite == 'browser':
            options.browserChrome = True
            flavor = 'browser-chrome'
        elif suite == 'metro':
            options.immersiveMode = True
            options.browserChrome = True
        elif suite == 'a11y':
            options.a11y = True
        elif suite == 'webapprt-content':
            options.webapprtContent = True
            options.app = self.get_webapp_runtime_path()
        elif suite == 'webapprt-chrome':
            options.webapprtChrome = True
            options.app = self.get_webapp_runtime_path()
            options.browserArgs.append("-test-mode")
        else:
            raise Exception('None or unrecognized mochitest suite type.')

        if dmd:
            options.dmdPath = self.bin_dir

        options.autorun = not no_autorun
        options.closeWhenDone = not keep_open
        options.slowscript = slowscript
        options.shuffle = shuffle
        options.consoleLevel = 'INFO'
        options.repeat = repeat
        options.runUntilFailure = run_until_failure
        options.runSlower = slow
        options.testingModulesDir = os.path.join(self.tests_dir, 'modules')
        options.extraProfileFiles.append(os.path.join(self.distdir, 'plugins'))
        options.symbolsPath = os.path.join(self.distdir, 'crashreporter-symbols')
        options.chunkByDir = chunk_by_dir
        options.totalChunks = total_chunks
        options.thisChunk = this_chunk
        options.jsdebugger = jsdebugger
        options.debugOnFailure = debug_on_failure
        options.startAt = start_at
        options.endAt = end_at
        options.e10s = e10s
        options.dumpAboutMemoryAfterTest = dump_about_memory_after_test
        options.dumpDMDAfterTest = dump_dmd_after_test
        options.dumpOutputDirectory = dump_output_directory

        options.failureFile = failure_file_path
        if install_extension != None:
            options.extensionsToInstall = [os.path.join(self.topsrcdir,install_extension)]

        for k, v in kwargs.iteritems():
            setattr(options, k, v)

        if test_paths:
            resolver = self._spawn(TestResolver)

            tests = list(resolver.resolve_tests(paths=test_paths, flavor=flavor,
                cwd=context.cwd))

            if not tests:
                print('No tests could be found in the path specified. Please '
                    'specify a path that is a test file or is a directory '
                    'containing tests.')
                return 1

            manifest = TestManifest()
            manifest.tests.extend(tests)

            options.manifestFile = manifest

        if rerun_failures:
            options.testManifest = failure_file_path

        if debugger:
            options.debugger = debugger

        if debugger_args:
            if options.debugger == None:
                print("--debugger-args passed, but no debugger specified.")
                return 1
            options.debuggerArgs = debugger_args

        options = opts.verifyOptions(options, runner)

        if options is None:
            raise Exception('mochitest option validator failed.')

        # We need this to enable colorization of output.
        self.log_manager.enable_unstructured()

        # Output processing is a little funky here. The old make targets
        # grepped the log output from TEST-UNEXPECTED-* and printed these lines
        # after test execution. Ideally the test runner would expose a Python
        # API for obtaining test results and we could just format failures
        # appropriately. Unfortunately, it doesn't yet do that. So, we capture
        # all output to a buffer then "grep" the buffer after test execution.
        # Bug 858197 tracks a Python API that would facilitate this.
        test_output = StringIO()
        handler = logging.StreamHandler(test_output)
        handler.addFilter(UnexpectedFilter())
        handler.setFormatter(StructuredHumanFormatter(0, write_times=False))
        logging.getLogger().addHandler(handler)

        result = runner.runTests(options)

        # Need to remove our buffering handler before we echo failures or else
        # it will catch them again!
        logging.getLogger().removeHandler(handler)
        self.log_manager.disable_unstructured()

        if test_output.getvalue():
            result = 1
            for line in test_output.getvalue().splitlines():
                self.log(logging.INFO, 'unexpected', {'msg': line}, '{msg}')

        return result


def MochitestCommand(func):
    """Decorator that adds shared command arguments to mochitest commands."""

    # This employs light Python magic. Keep in mind a decorator is just a
    # function that takes a function, does something with it, then returns a
    # (modified) function. Here, we chain decorators onto the passed in
    # function.

    debugger = CommandArgument('--debugger', '-d', metavar='DEBUGGER',
        help='Debugger binary to run test in. Program name or path.')
    func = debugger(func)

    debugger_args = CommandArgument('--debugger-args',
        metavar='DEBUGGER_ARGS', help='Arguments to pass to the debugger.')
    func = debugger_args(func)

    # Bug 933807 introduced JS_DISABLE_SLOW_SCRIPT_SIGNALS to avoid clever
    # segfaults induced by the slow-script-detecting logic for Ion/Odin JITted
    # code. If we don't pass this, the user will need to periodically type
    # "continue" to (safely) resume execution. There are ways to implement
    # automatic resuming; see the bug.
    slowscript = CommandArgument('--slowscript', action='store_true',
        help='Do not set the JS_DISABLE_SLOW_SCRIPT_SIGNALS env variable; when not set, recoverable but misleading SIGSEGV instances may occur in Ion/Odin JIT code')
    func = slowscript(func)

    shuffle = CommandArgument('--shuffle', action='store_true',
        help='Shuffle execution order.')
    func = shuffle(func)

    keep_open = CommandArgument('--keep-open', action='store_true',
        help='Keep the browser open after tests complete.')
    func = keep_open(func)

    rerun = CommandArgument('--rerun-failures', action='store_true',
        help='Run only the tests that failed during the last test run.')
    func = rerun(func)

    autorun = CommandArgument('--no-autorun', action='store_true',
        help='Do not starting running tests automatically.')
    func = autorun(func)

    repeat = CommandArgument('--repeat', type=int, default=0,
        help='Repeat the test the given number of times.')
    func = repeat(func)

    runUntilFailure = CommandArgument("--run-until-failure", action='store_true',
        help='Run tests repeatedly and stops on the first time a test fails. ' \
             'Default cap is 30 runs, which can be overwritten ' \
             'with the --repeat parameter.')
    func = runUntilFailure(func)

    slow = CommandArgument('--slow', action='store_true',
        help='Delay execution between tests.')
    func = slow(func)

    end_at = CommandArgument('--end-at', type=str,
        help='Stop running the test sequence at this test.')
    func = end_at(func)

    start_at = CommandArgument('--start-at', type=str,
        help='Start running the test sequence at this test.')
    func = start_at(func)

    chunk_dir = CommandArgument('--chunk-by-dir', type=int,
        help='Group tests together in chunks by this many top directories.')
    func = chunk_dir(func)

    chunk_total = CommandArgument('--total-chunks', type=int,
        help='Total number of chunks to split tests into.')
    func = chunk_total(func)

    this_chunk = CommandArgument('--this-chunk', type=int,
        help='If running tests by chunks, the number of the chunk to run.')
    func = this_chunk(func)

    hide_subtests = CommandArgument('--hide-subtests', action='store_true',
        help='If specified, will only log subtest results on failure or timeout.')
    func = hide_subtests(func)

    debug_on_failure = CommandArgument('--debug-on-failure', action='store_true',
        help='Breaks execution and enters the JS debugger on a test failure. ' \
             'Should be used together with --jsdebugger.')
    func = debug_on_failure(func)

    jsdebugger = CommandArgument('--jsdebugger', action='store_true',
        help='Start the browser JS debugger before running the test. Implies --no-autorun.')
    func = jsdebugger(func)

    this_chunk = CommandArgument('--e10s', action='store_true',
        help='Run tests with electrolysis preferences and test filtering enabled.')
    func = this_chunk(func)

    dmd = CommandArgument('--dmd', action='store_true',
        help='Run tests with DMD active.')
    func = dmd(func)

    dumpAboutMemory = CommandArgument('--dump-about-memory-after-test', action='store_true',
        help='Dump an about:memory log after every test.')
    func = dumpAboutMemory(func)

    dumpDMD = CommandArgument('--dump-dmd-after-test', action='store_true',
        help='Dump a DMD log after every test.')
    func = dumpDMD(func)

    dumpOutputDirectory = CommandArgument('--dump-output-directory', action='store',
        help='Specifies the directory in which to place dumped memory reports.')
    func = dumpOutputDirectory(func)

    path = CommandArgument('test_paths', default=None, nargs='*',
        metavar='TEST',
        help='Test to run. Can be specified as a single file, a ' \
            'directory, or omitted. If omitted, the entire test suite is ' \
            'executed.')
    func = path(func)

    install_extension = CommandArgument('--install-extension',
        help='Install given extension before running selected tests. ' \
            'Parameter is a path to xpi file.')
    func = install_extension(func)

    return func

def B2GCommand(func):
    """Decorator that adds shared command arguments to b2g mochitest commands."""

    busybox = CommandArgument('--busybox', default=None,
        help='Path to busybox binary to install on device')
    func = busybox(func)

    logcatdir = CommandArgument('--logcat-dir', default=None,
        help='directory to store logcat dump files')
    func = logcatdir(func)

    profile = CommandArgument('--profile', default=None,
        help='for desktop testing, the path to the \
              gaia profile to use')
    func = profile(func)

    geckopath = CommandArgument('--gecko-path', default=None,
        help='the path to a gecko distribution that should \
              be installed on the emulator prior to test')
    func = geckopath(func)

    nowindow = CommandArgument('--no-window', action='store_true', default=False,
        help='Pass --no-window to the emulator')
    func = nowindow(func)

    sdcard = CommandArgument('--sdcard', default="10MB",
        help='Define size of sdcard: 1MB, 50MB...etc')
    func = sdcard(func)

    emulator = CommandArgument('--emulator', default='arm',
        help='Architecture of emulator to use: x86 or arm')
    func = emulator(func)

    marionette = CommandArgument('--marionette', default=None,
        help='host:port to use when connecting to Marionette')
    func = marionette(func)

    chunk_total = CommandArgument('--total-chunks', type=int,
        help='Total number of chunks to split tests into.')
    func = chunk_total(func)

    this_chunk = CommandArgument('--this-chunk', type=int,
        help='If running tests by chunks, the number of the chunk to run.')
    func = this_chunk(func)

    hide_subtests = CommandArgument('--hide-subtests', action='store_true',
        help='If specified, will only log subtest results on failure or timeout.')
    func = hide_subtests(func)

    path = CommandArgument('test_paths', default=None, nargs='*',
        metavar='TEST',
        help='Test to run. Can be specified as a single file, a ' \
            'directory, or omitted. If omitted, the entire test suite is ' \
            'executed.')
    func = path(func)

    return func



@CommandProvider
class MachCommands(MachCommandBase):
    @Command('mochitest-plain', category='testing',
        conditions=[conditions.is_firefox],
        description='Run a plain mochitest.')
    @MochitestCommand
    def run_mochitest_plain(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'plain', **kwargs)

    @Command('mochitest-chrome', category='testing',
        conditions=[conditions.is_firefox],
        description='Run a chrome mochitest.')
    @MochitestCommand
    def run_mochitest_chrome(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'chrome', **kwargs)

    @Command('mochitest-browser', category='testing',
        conditions=[conditions.is_firefox],
        description='Run a mochitest with browser chrome.')
    @MochitestCommand
    def run_mochitest_browser(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'browser', **kwargs)

    @Command('mochitest-metro', category='testing',
        conditions=[conditions.is_firefox],
        description='Run a mochitest with metro browser chrome.')
    @MochitestCommand
    def run_mochitest_metro(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'metro', **kwargs)

    @Command('mochitest-a11y', category='testing',
        conditions=[conditions.is_firefox],
        description='Run an a11y mochitest.')
    @MochitestCommand
    def run_mochitest_a11y(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'a11y', **kwargs)

    @Command('webapprt-test-chrome', category='testing',
        conditions=[conditions.is_firefox],
        description='Run a webapprt chrome mochitest.')
    @MochitestCommand
    def run_mochitest_webapprt_chrome(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'webapprt-chrome', **kwargs)

    @Command('webapprt-test-content', category='testing',
        conditions=[conditions.is_firefox],
        description='Run a webapprt content mochitest.')
    @MochitestCommand
    def run_mochitest_webapprt_content(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'webapprt-content', **kwargs)

    def run_mochitest(self, test_paths, flavor, **kwargs):
        from mozbuild.controller.building import BuildDriver

        self._ensure_state_subdir_exists('.')

        driver = self._spawn(BuildDriver)
        driver.install_tests(remove=False)

        mochitest = self._spawn(MochitestRunner)

        return mochitest.run_desktop_test(self._mach_context,
            test_paths=test_paths, suite=flavor, **kwargs)


# TODO For now b2g commands will only work with the emulator,
# they should be modified to work with all devices.
def is_emulator(cls):
    """Emulator needs to be configured."""
    return cls.device_name in ('emulator', 'emulator-jb')


@CommandProvider
class B2GCommands(MachCommandBase):
    """So far these are only mochitest plain. They are
    implemented separately because their command lines
    are completely different.
    """
    def __init__(self, context):
        MachCommandBase.__init__(self, context)

        for attr in ('b2g_home', 'xre_path', 'device_name'):
            setattr(self, attr, getattr(context, attr, None))

    @Command('mochitest-remote', category='testing',
        description='Run a remote mochitest.',
        conditions=[conditions.is_b2g, is_emulator])
    @B2GCommand
    def run_mochitest_remote(self, test_paths, **kwargs):
        from mozbuild.controller.building import BuildDriver

        self._ensure_state_subdir_exists('.')

        driver = self._spawn(BuildDriver)
        driver.install_tests(remove=False)

        mochitest = self._spawn(MochitestRunner)
        return mochitest.run_b2g_test(b2g_home=self.b2g_home,
                xre_path=self.xre_path, test_paths=test_paths, **kwargs)

    @Command('mochitest-b2g-desktop', category='testing',
        conditions=[conditions.is_b2g_desktop],
        description='Run a b2g desktop mochitest.')
    @B2GCommand
    def run_mochitest_b2g_desktop(self, test_paths, **kwargs):
        from mozbuild.controller.building import BuildDriver

        self._ensure_state_subdir_exists('.')

        driver = self._spawn(BuildDriver)
        driver.install_tests(remove=False)

        mochitest = self._spawn(MochitestRunner)
        return mochitest.run_b2g_test(test_paths=test_paths, **kwargs)

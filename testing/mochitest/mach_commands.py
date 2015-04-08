# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import argparse
import logging
import mozpack.path as mozpath
import os
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


from mozlog import structured

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

ENG_BUILD_REQUIRED = '''
The %s command requires an engineering build. It may be the case that
VARIANT=user or PRODUCTION=1 were set. Try re-building with VARIANT=eng:

    $ VARIANT=eng ./build.sh

There should be an app called 'test-container.gaiamobile.org' located in
%s.
'''.lstrip()

# Maps test flavors to mochitest suite type.
FLAVORS = {
    'mochitest': 'plain',
    'chrome': 'chrome',
    'browser-chrome': 'browser',
    'jetpack-package': 'jetpack-package',
    'jetpack-addon': 'jetpack-addon',
    'a11y': 'a11y',
    'webapprt-chrome': 'webapprt-chrome',
}


class MochitestRunner(MozbuildObject):

    """Easily run mochitests.

    This currently contains just the basics for running mochitests. We may want
    to hook up result parsing, etc.
    """

    def get_webapp_runtime_path(self):
        import mozinfo
        appname = 'webapprt-stub' + mozinfo.info.get('bin_suffix', '')
        if sys.platform.startswith('darwin'):
            appname = os.path.join(
                self.distdir,
                self.substs['MOZ_MACBUNDLE_NAME'],
                'Contents',
                'Resources',
                appname)
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
        self.mochitest_dir = os.path.join(
            self.tests_dir,
            'testing',
            'mochitest')
        self.bin_dir = os.path.join(self.topobjdir, 'dist', 'bin')

    def run_b2g_test(
            self,
            test_paths=None,
            b2g_home=None,
            xre_path=None,
            total_chunks=None,
            this_chunk=None,
            no_window=None,
            repeat=0,
            run_until_failure=False,
            chrome=False,
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
        options = parser.parse_args([])

        if test_path:
            if chrome:
                test_root_file = mozpath.join(
                    self.mochitest_dir,
                    'chrome',
                    test_path)
            else:
                test_root_file = mozpath.join(
                    self.mochitest_dir,
                    'tests',
                    test_path)
            if not os.path.exists(test_root_file):
                print(
                    'Specified test path does not exist: %s' %
                    test_root_file)
                return 1
            options.testPath = test_path

        for k, v in kwargs.iteritems():
            setattr(options, k, v)
        options.noWindow = no_window
        options.totalChunks = total_chunks
        options.thisChunk = this_chunk
        options.repeat = repeat
        options.runUntilFailure = run_until_failure

        options.symbolsPath = os.path.join(
            self.distdir,
            'crashreporter-symbols')

        options.consoleLevel = 'INFO'
        if conditions.is_b2g_desktop(self):
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
        options.logdir = self.mochitest_dir
        options.httpdPath = self.mochitest_dir
        options.xrePath = xre_path
        options.chrome = chrome
        return mochitest.run_remote_mochitests(parser, options)

    def run_desktop_test(
            self,
            context,
            suite=None,
            test_paths=None,
            debugger=None,
            debugger_args=None,
            slowscript=False,
            screenshot_on_fail=False,
            shuffle=False,
            closure_behaviour='auto',
            rerun_failures=False,
            no_autorun=False,
            repeat=0,
            run_until_failure=False,
            slow=False,
            chunk_by_dir=0,
            chunk_by_runtime=False,
            total_chunks=None,
            this_chunk=None,
            extraPrefs=[],
            jsdebugger=False,
            debug_on_failure=False,
            start_at=None,
            end_at=None,
            e10s=False,
            strict_content_sandbox=False,
            nested_oop=False,
            dmd=False,
            dump_output_directory=None,
            dump_about_memory_after_test=False,
            dump_dmd_after_test=False,
            install_extension=None,
            quiet=False,
            environment=[],
            app_override=None,
            bisectChunk=None,
            runByDir=False,
            useTestMediaDevices=False,
            timeout=None,
            max_timeouts=None,
            **kwargs):
        """Runs a mochitest.

        test_paths are path to tests. They can be a relative path from the
        top source directory, an absolute filename, or a directory containing
        test files.

        suite is the type of mochitest to run. It can be one of ('plain',
        'chrome', 'browser', 'metro', 'a11y', 'jetpack-package', 'jetpack-addon').

        debugger is a program name or path to a binary (presumably a debugger)
        to run the test in. e.g. 'gdb'

        debugger_args are the arguments passed to the debugger.

        slowscript is true if the user has requested the SIGSEGV mechanism of
        invoking the slow script dialog.

        shuffle is whether test order should be shuffled (defaults to false).

        closure_behaviour denotes whether to keep the browser open after tests
        complete.
        """
        if rerun_failures and test_paths:
            print('Cannot specify both --rerun-failures and a test path.')
            return 1

        # Make absolute paths relative before calling os.chdir() below.
        if test_paths:
            test_paths = [self._wrap_path_argument(
                p).relpath() if os.path.isabs(p) else p for p in test_paths]

        failure_file_path = os.path.join(
            self.statedir,
            'mochitest_failures.json')

        if rerun_failures and not os.path.exists(failure_file_path):
            print('No failure file present. Did you run mochitests before?')
            return 1

        # runtests.py is ambiguous, so we load the file/module manually.
        if 'mochitest' not in sys.modules:
            import imp
            path = os.path.join(self.mochitest_dir, 'runtests.py')
            with open(path, 'r') as fh:
                imp.load_module('mochitest', fh, path,
                                ('.py', 'r', imp.PY_SOURCE))

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

        opts = mochitest.MochitestOptions()
        options = opts.parse_args([])

        options.subsuite = ''
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
        elif suite == 'devtools':
            options.browserChrome = True
            options.subsuite = 'devtools'
        elif suite == 'jetpack-package':
            options.jetpackPackage = True
        elif suite == 'jetpack-addon':
            options.jetpackAddon = True
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
        options.closeWhenDone = closure_behaviour != 'open'
        options.slowscript = slowscript
        options.screenshotOnFail = screenshot_on_fail
        options.shuffle = shuffle
        options.consoleLevel = 'INFO'
        options.repeat = repeat
        options.runUntilFailure = run_until_failure
        options.runSlower = slow
        options.testingModulesDir = os.path.join(self.tests_dir, 'modules')
        options.extraProfileFiles.append(os.path.join(self.distdir, 'plugins'))
        options.symbolsPath = os.path.join(
            self.distdir,
            'crashreporter-symbols')
        options.chunkByDir = chunk_by_dir
        options.chunkByRuntime = chunk_by_runtime
        options.totalChunks = total_chunks
        options.thisChunk = this_chunk
        options.jsdebugger = jsdebugger
        options.debugOnFailure = debug_on_failure
        options.startAt = start_at
        options.endAt = end_at
        options.e10s = e10s
        options.strictContentSandbox = strict_content_sandbox
        options.nested_oop = nested_oop
        options.dumpAboutMemoryAfterTest = dump_about_memory_after_test
        options.dumpDMDAfterTest = dump_dmd_after_test
        options.dumpOutputDirectory = dump_output_directory
        options.quiet = quiet
        options.environment = environment
        options.extraPrefs = extraPrefs
        options.bisectChunk = bisectChunk
        options.runByDir = runByDir
        options.useTestMediaDevices = useTestMediaDevices
        if timeout:
            options.timeout = int(timeout)
        if max_timeouts:
            options.maxTimeouts = int(max_timeouts)

        options.failureFile = failure_file_path
        if install_extension is not None:
            options.extensionsToInstall = [
                os.path.join(
                    self.topsrcdir,
                    install_extension)]

        for k, v in kwargs.iteritems():
            setattr(options, k, v)

        if test_paths:
            resolver = self._spawn(TestResolver)

            tests = list(
                resolver.resolve_tests(
                    paths=test_paths,
                    flavor=flavor))

            if not tests:
                print('No tests could be found in the path specified. Please '
                      'specify a path that is a test file or is a directory '
                      'containing tests.')
                return 1

            manifest = TestManifest()
            manifest.tests.extend(tests)

            if len(
                    tests) == 1 and closure_behaviour == 'auto' and suite == 'plain':
                options.closeWhenDone = False

            options.manifestFile = manifest

        if rerun_failures:
            options.testManifest = failure_file_path

        if debugger:
            options.debugger = debugger

        if debugger_args:
            if options.debugger is None:
                print("--debugger-args passed, but no debugger specified.")
                return 1
            options.debuggerArgs = debugger_args

        if app_override:
            if app_override == "dist":
                options.app = self.get_binary_path(where='staged-package')
            elif app_override:
                options.app = app_override
            if options.gmp_path is None:
                # Need to fix the location of gmp_fake which might not be
                # shipped in the binary
                bin_path = self.get_binary_path()
                options.gmp_path = os.path.join(
                    os.path.dirname(bin_path),
                    'gmp-fake',
                    '1.0')
                options.gmp_path += os.pathsep
                options.gmp_path += os.path.join(
                    os.path.dirname(bin_path),
                    'gmp-clearkey',
                    '0.1')

        logger_options = {
            key: value for key,
            value in vars(options).iteritems() if key.startswith('log')}
        runner = mochitest.Mochitest(logger_options)
        options = opts.verifyOptions(options, runner)

        if options is None:
            raise Exception('mochitest option validator failed.')

        # We need this to enable colorization of output.
        self.log_manager.enable_unstructured()

        result = runner.runTests(options)

        self.log_manager.disable_unstructured()
        if runner.message_logger.errors:
            result = 1
            runner.message_logger.logger.warning("The following tests failed:")
            for error in runner.message_logger.errors:
                runner.message_logger.logger.log_raw(error)

        runner.message_logger.finish()

        return result


def add_mochitest_general_args(parser):
    parser.add_argument(
        '--debugger',
        '-d',
        metavar='DEBUGGER',
        help='Debugger binary to run test in. Program name or path.')

    parser.add_argument(
        '--debugger-args',
        metavar='DEBUGGER_ARGS',
        help='Arguments to pass to the debugger.')

    # Bug 933807 introduced JS_DISABLE_SLOW_SCRIPT_SIGNALS to avoid clever
    # segfaults induced by the slow-script-detecting logic for Ion/Odin JITted
    # code. If we don't pass this, the user will need to periodically type
    # "continue" to (safely) resume execution. There are ways to implement
    # automatic resuming; see the bug.
    parser.add_argument(
        '--slowscript',
        action='store_true',
        help='Do not set the JS_DISABLE_SLOW_SCRIPT_SIGNALS env variable; when not set, recoverable but misleading SIGSEGV instances may occur in Ion/Odin JIT code')

    parser.add_argument(
        '--screenshot-on-fail',
        action='store_true',
        help='Take screenshots on all test failures. Set $MOZ_UPLOAD_DIR to a directory for storing the screenshots.')

    parser.add_argument(
        '--shuffle', action='store_true',
        help='Shuffle execution order.')

    parser.add_argument(
        '--keep-open',
        action='store_const',
        dest='closure_behaviour',
        const='open',
        default='auto',
        help='Always keep the browser open after tests complete.')

    parser.add_argument(
        '--auto-close',
        action='store_const',
        dest='closure_behaviour',
        const='close',
        default='auto',
        help='Always close the browser after tests complete.')

    parser.add_argument(
        '--rerun-failures',
        action='store_true',
        help='Run only the tests that failed during the last test run.')

    parser.add_argument(
        '--no-autorun',
        action='store_true',
        help='Do not starting running tests automatically.')

    parser.add_argument(
        '--repeat', type=int, default=0,
        help='Repeat the test the given number of times.')

    parser.add_argument(
        "--run-until-failure",
        action='store_true',
        help='Run tests repeatedly and stops on the first time a test fails. '
        'Default cap is 30 runs, which can be overwritten '
        'with the --repeat parameter.')

    parser.add_argument(
        '--slow', action='store_true',
        help='Delay execution between tests.')

    parser.add_argument(
        '--end-at',
        type=str,
        help='Stop running the test sequence at this test.')

    parser.add_argument(
        '--start-at',
        type=str,
        help='Start running the test sequence at this test.')

    parser.add_argument(
        '--chunk-by-dir',
        type=int,
        help='Group tests together in chunks by this many top directories.')

    parser.add_argument(
        '--chunk-by-runtime',
        action='store_true',
        help="Group tests such that each chunk has roughly the same runtime.")

    parser.add_argument(
        '--total-chunks',
        type=int,
        help='Total number of chunks to split tests into.')

    parser.add_argument(
        '--this-chunk',
        type=int,
        help='If running tests by chunks, the number of the chunk to run.')

    parser.add_argument(
        '--debug-on-failure',
        action='store_true',
        help='Breaks execution and enters the JS debugger on a test failure. '
        'Should be used together with --jsdebugger.')

    parser.add_argument(
        '--setpref', default=[], action='append',
        metavar='PREF=VALUE', dest='extraPrefs',
        help='defines an extra user preference')

    parser.add_argument(
        '--jsdebugger',
        action='store_true',
        help='Start the browser JS debugger before running the test. Implies --no-autorun.')

    parser.add_argument(
        '--e10s',
        action='store_true',
        help='Run tests with electrolysis preferences and test filtering enabled.')

    parser.add_argument(
        '--strict-content-sandbox',
        action='store_true',
        help='Run tests with a more strict content sandbox (Windows only).')

    parser.add_argument(
        '--nested_oop',
        action='store_true',
        help='Run tests with nested oop preferences and test filtering enabled.')

    parser.add_argument(
        '--dmd', action='store_true',
        help='Run tests with DMD active.')

    parser.add_argument(
        '--dump-about-memory-after-test',
        action='store_true',
        help='Dump an about:memory log after every test.')

    parser.add_argument(
        '--dump-dmd-after-test', action='store_true',
        help='Dump a DMD log after every test.')

    parser.add_argument(
        '--dump-output-directory',
        action='store',
        help='Specifies the directory in which to place dumped memory reports.')

    parser.add_argument(
        'test_paths',
        default=None,
        nargs='*',
        metavar='TEST',
        help='Test to run. Can be specified as a single file, a '
        'directory, or omitted. If omitted, the entire test suite is '
        'executed.')

    parser.add_argument(
        '--install-extension',
        help='Install given extension before running selected tests. '
        'Parameter is a path to xpi file.')

    parser.add_argument(
        '--quiet',
        default=False,
        action='store_true',
        help='Do not print test log lines unless a failure occurs.')

    parser.add_argument(
        '--setenv',
        default=[],
        action='append',
        metavar='NAME=VALUE',
        dest='environment',
        help="Sets the given variable in the application's environment")

    parser.add_argument(
        '--run-by-dir',
        default=False,
        action='store_true',
        dest='runByDir',
        help='Run each directory in a single browser instance with a fresh profile.')

    parser.add_argument(
        '--bisect-chunk',
        type=str,
        dest='bisectChunk',
        help='Specify the failing test name to find the previous tests that may be causing the failure.')

    parser.add_argument(
        '--use-test-media-devices',
        default=False,
        action='store_true',
        dest='useTestMediaDevices',
        help='Use test media device drivers for media testing.')

    parser.add_argument(
        '--app-override',
        default=None,
        action='store',
        help="Override the default binary used to run tests with the path you provide, e.g. "
        " --app-override /usr/bin/firefox . "
        "If you have run ./mach package beforehand, you can specify 'dist' to "
        "run tests against the distribution bundle's binary.")

    parser.add_argument(
        '--timeout',
        default=None,
        help='The per-test timeout time in seconds (default: 60 seconds)')

    parser.add_argument(
        '--max-timeouts', default=None,
        help='The maximum number of timeouts permitted before halting testing')

    parser.add_argument(
        "--tag",
        dest='test_tags', action='append',
        help="Filter out tests that don't have the given tag. Can be used "
             "multiple times in which case the test must contain at least one "
             "of the given tags.")

    return parser

def add_mochitest_b2g_args(parser):
    parser.add_argument(
        '--busybox',
        default=None,
        help='Path to busybox binary to install on device')

    parser.add_argument(
        '--logdir', default=None,
        help='directory to store log files')

    parser.add_argument(
        '--profile', default=None,
        help='for desktop testing, the path to the \
              gaia profile to use')

    parser.add_argument(
        '--gecko-path', default=None,
        help='the path to a gecko distribution that should \
              be installed on the emulator prior to test')

    parser.add_argument(
        '--no-window',
        action='store_true',
        default=False,
        help='Pass --no-window to the emulator')

    parser.add_argument(
        '--sdcard', default="10MB",
        help='Define size of sdcard: 1MB, 50MB...etc')

    parser.add_argument(
        '--marionette',
        default=None,
        help='host:port to use when connecting to Marionette')

    return parser


def setup_argument_parser():
    parser = argparse.ArgumentParser()

    general_args = parser.add_argument_group('Mochitest Arguments',
        'Arguments that apply to all versions of mochitest.')
    general_args = add_mochitest_general_args(general_args)

    b2g_args = parser.add_argument_group('B2G Arguments', 'Arguments specific \
        to running mochitest on B2G devices and emulator')
    b2g_args = add_mochitest_b2g_args(b2g_args)

    structured.commandline.add_logging_group(parser)
    return parser

_st_parser = setup_argument_parser()


# condition filters

def is_platform_in(*platforms):
    def is_platform_supported(cls):
        for p in platforms:
            c = getattr(conditions, 'is_{}'.format(p), None)
            if c and c(cls):
                return True
        return False

    is_platform_supported.__doc__ = 'Must have a {} build.'.format(
        ' or '.join(platforms))
    return is_platform_supported


@CommandProvider
class MachCommands(MachCommandBase):

    def __init__(self, context):
        MachCommandBase.__init__(self, context)

        for attr in ('b2g_home', 'xre_path', 'device_name', 'target_out'):
            setattr(self, attr, getattr(context, attr, None))

    @Command(
        'mochitest-plain',
        category='testing',
        conditions=[is_platform_in('firefox', 'mulet', 'b2g', 'b2g_desktop')],
        description='Run a plain mochitest (integration test, plain web page).',
        parser=_st_parser)
    def run_mochitest_plain(self, test_paths, **kwargs):
        if is_platform_in('firefox', 'mulet')(self):
            return self.run_mochitest(test_paths, 'plain', **kwargs)
        elif conditions.is_emulator(self):
            return self.run_mochitest_remote(test_paths, **kwargs)
        elif conditions.is_b2g_desktop(self):
            return self.run_b2g_desktop(test_paths, **kwargs)

    @Command(
        'mochitest-chrome',
        category='testing',
        conditions=[is_platform_in('firefox', 'emulator')],
        description='Run a chrome mochitest (integration test with some XUL).',
        parser=_st_parser)
    def run_mochitest_chrome(self, test_paths, **kwargs):
        if conditions.is_firefox(self):
            return self.run_mochitest(test_paths, 'chrome', **kwargs)
        elif conditions.is_b2g(self) and conditions.is_emulator(self):
            return self.run_mochitest_remote(test_paths, chrome=True, **kwargs)

    @Command(
        'mochitest-browser',
        category='testing',
        conditions=[conditions.is_firefox],
        description='Run a mochitest with browser chrome (integration test with a standard browser).',
        parser=_st_parser)
    def run_mochitest_browser(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'browser', **kwargs)

    @Command(
        'mochitest-devtools',
        category='testing',
        conditions=[conditions.is_firefox],
        description='Run a devtools mochitest with browser chrome (integration test with a standard browser with the devtools frame).',
        parser=_st_parser)
    def run_mochitest_devtools(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'devtools', **kwargs)

    @Command('jetpack-package', category='testing',
             conditions=[conditions.is_firefox],
             description='Run a jetpack package test.')
    def run_mochitest_jetpack_package(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'jetpack-package', **kwargs)

    @Command('jetpack-addon', category='testing',
             conditions=[conditions.is_firefox],
             description='Run a jetpack addon test.')
    def run_mochitest_jetpack_addon(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'jetpack-addon', **kwargs)

    @Command(
        'mochitest-metro',
        category='testing',
        conditions=[conditions.is_firefox],
        description='Run a mochitest with metro browser chrome (tests for Windows touch interface).',
        parser=_st_parser)
    def run_mochitest_metro(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'metro', **kwargs)

    @Command('mochitest-a11y', category='testing',
             conditions=[conditions.is_firefox],
             description='Run an a11y mochitest (accessibility tests).',
             parser=_st_parser)
    def run_mochitest_a11y(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'a11y', **kwargs)

    @Command(
        'webapprt-test-chrome',
        category='testing',
        conditions=[conditions.is_firefox],
        description='Run a webapprt chrome mochitest (Web App Runtime with the browser chrome).',
        parser=_st_parser)
    def run_mochitest_webapprt_chrome(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'webapprt-chrome', **kwargs)

    @Command(
        'webapprt-test-content',
        category='testing',
        conditions=[conditions.is_firefox],
        description='Run a webapprt content mochitest (Content rendering of the Web App Runtime).',
        parser=_st_parser)
    def run_mochitest_webapprt_content(self, test_paths, **kwargs):
        return self.run_mochitest(test_paths, 'webapprt-content', **kwargs)

    @Command('mochitest', category='testing',
             conditions=[conditions.is_firefox],
             description='Run any flavor of mochitest (integration test).',
             parser=_st_parser)
    @CommandArgument('-f', '--flavor', choices=FLAVORS.keys(),
                     help='Only run tests of this flavor.')
    def run_mochitest_general(self, test_paths, flavor=None, test_objects=None,
                              **kwargs):
        self._preruntest()

        from mozbuild.testing import TestResolver

        if test_objects:
            tests = test_objects
        else:
            resolver = self._spawn(TestResolver)
            tests = list(resolver.resolve_tests(paths=test_paths,
                                                cwd=self._mach_context.cwd))

        # Our current approach is to group the tests by suite and then perform
        # an invocation for each suite. Ideally, this would be done
        # automatically inside of core mochitest code. But it wasn't designed
        # to do that.
        #
        # This does mean our output is less than ideal. When running tests from
        # multiple suites, we see redundant summary lines. Hopefully once we
        # have better machine readable output coming from mochitest land we can
        # aggregate that here and improve the output formatting.

        suites = {}
        for test in tests:
            # Filter out non-mochitests.
            if test['flavor'] not in FLAVORS:
                continue

            if flavor and test['flavor'] != flavor:
                continue

            suite = FLAVORS[test['flavor']]
            suites.setdefault(suite, []).append(test)

        mochitest = self._spawn(MochitestRunner)
        overall = None
        for suite, tests in sorted(suites.items()):
            result = mochitest.run_desktop_test(
                self._mach_context,
                test_paths=[
                    test['file_relpath'] for test in tests],
                suite=suite,
                **kwargs)
            if result:
                overall = result

        return overall

    def _preruntest(self):
        from mozbuild.controller.building import BuildDriver

        self._ensure_state_subdir_exists('.')

        driver = self._spawn(BuildDriver)
        driver.install_tests(remove=False)

    def run_mochitest(self, test_paths, flavor, **kwargs):
        self._preruntest()

        mochitest = self._spawn(MochitestRunner)

        return mochitest.run_desktop_test(
            self._mach_context,
            test_paths=test_paths,
            suite=flavor,
            **kwargs)

    def run_mochitest_remote(self, test_paths, **kwargs):
        if self.target_out:
            host_webapps_dir = os.path.join(
                self.target_out,
                'data',
                'local',
                'webapps')
            if not os.path.isdir(
                os.path.join(
                    host_webapps_dir,
                    'test-container.gaiamobile.org')):
                print(
                    ENG_BUILD_REQUIRED %
                    ('mochitest-remote', host_webapps_dir))
                return 1

        from mozbuild.controller.building import BuildDriver

        if self.device_name.startswith('emulator'):
            emulator = 'arm'
            if 'x86' in self.device_name:
                emulator = 'x86'
            kwargs['emulator'] = emulator

        self._ensure_state_subdir_exists('.')

        driver = self._spawn(BuildDriver)
        driver.install_tests(remove=False)

        mochitest = self._spawn(MochitestRunner)
        return mochitest.run_b2g_test(
            b2g_home=self.b2g_home,
            xre_path=self.xre_path,
            test_paths=test_paths,
            **kwargs)

    def run_mochitest_b2g_desktop(self, test_paths, **kwargs):
        kwargs['profile'] = kwargs.get(
            'profile') or os.environ.get('GAIA_PROFILE')
        if not kwargs['profile'] or not os.path.isdir(kwargs['profile']):
            print(GAIA_PROFILE_NOT_FOUND % 'mochitest-b2g-desktop')
            return 1

        if os.path.isfile(os.path.join(kwargs['profile'], 'extensions',
                                       'httpd@gaiamobile.org')):
            print(GAIA_PROFILE_IS_DEBUG % ('mochitest-b2g-desktop',
                                           kwargs['profile']))
            return 1

        from mozbuild.controller.building import BuildDriver

        self._ensure_state_subdir_exists('.')

        driver = self._spawn(BuildDriver)
        driver.install_tests(remove=False)

        mochitest = self._spawn(MochitestRunner)
        return mochitest.run_b2g_test(test_paths=test_paths, **kwargs)


@CommandProvider
class AndroidCommands(MachCommandBase):

    @Command('robocop', category='testing',
             conditions=[conditions.is_android],
             description='Run a Robocop test.')
    @CommandArgument(
        'test_path',
        default=None,
        nargs='?',
        metavar='TEST',
        help='Test to run. Can be specified as a Robocop test name (like "testLoad"), '
        'or omitted. If omitted, the entire test suite is executed.')
    def run_robocop(self, test_path):
        self.tests_dir = os.path.join(self.topobjdir, '_tests')
        self.mochitest_dir = os.path.join(
            self.tests_dir,
            'testing',
            'mochitest')
        import imp
        path = os.path.join(self.mochitest_dir, 'runtestsremote.py')
        with open(path, 'r') as fh:
            imp.load_module('runtestsremote', fh, path,
                            ('.py', 'r', imp.PY_SOURCE))
        import runtestsremote

        MOZ_HOST_BIN = os.environ.get('MOZ_HOST_BIN')
        if not MOZ_HOST_BIN:
            print('environment variable MOZ_HOST_BIN must be set to a directory containing host xpcshell')
            return 1
        elif not os.path.isdir(MOZ_HOST_BIN):
            print('$MOZ_HOST_BIN does not specify a directory')
            return 1
        elif not os.path.isfile(os.path.join(MOZ_HOST_BIN, 'xpcshell')):
            print('$MOZ_HOST_BIN/xpcshell does not exist')
            return 1

        args = [
            '--xre-path=' + MOZ_HOST_BIN,
            '--dm_trans=adb',
            '--deviceIP=',
            '--console-level=INFO',
            '--app=' +
            self.substs['ANDROID_PACKAGE_NAME'],
            '--robocop-apk=' +
            os.path.join(
                self.topobjdir,
                'build',
                'mobile',
                'robocop',
                'robocop-debug.apk'),
            '--robocop-ini=' +
            os.path.join(
                self.topobjdir,
                'build',
                'mobile',
                'robocop',
                'robocop.ini'),
            '--log-mach=-',
        ]

        if test_path:
            args.append('--test-path=%s' % test_path)

        sys.exit(runtestsremote.main(args))

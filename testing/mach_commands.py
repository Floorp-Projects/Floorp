# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os
import sys
import subprocess

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SettingsProvider,
    SubCommand,
)

from mozbuild.base import (
    BuildEnvironmentNotFoundException,
    MachCommandBase,
    MachCommandConditions as conditions,
)

UNKNOWN_TEST = '''
I was unable to find tests from the given argument(s).

You should specify a test directory, filename, test suite name, or
abbreviation. If no arguments are given, there must be local file
changes and corresponding IMPACTED_TESTS annotations in moz.build
files relevant to those files.

It's possible my little brain doesn't know about the type of test you are
trying to execute. If you suspect this, please request support by filing
a bug at
https://bugzilla.mozilla.org/enter_bug.cgi?product=Testing&component=General.
'''.strip()

UNKNOWN_FLAVOR = '''
I know you are trying to run a %s%s test. Unfortunately, I can't run those
tests yet. Sorry!
'''.strip()

TEST_HELP = '''
Test or tests to run. Tests can be specified by filename, directory, suite
name or suite alias.

The following test suites and aliases are supported: {}
'''.strip()


@SettingsProvider
class TestConfig(object):

    @classmethod
    def config_settings(cls):
        from mozlog.commandline import log_formatters
        from mozlog.structuredlog import log_levels
        format_desc = "The default format to use when running tests with `mach test`."
        format_choices = log_formatters.keys()
        level_desc = "The default log level to use when running tests with `mach test`."
        level_choices = [l.lower() for l in log_levels]
        return [
            ('test.format', 'string', format_desc, 'mach', {'choices': format_choices}),
            ('test.level', 'string', level_desc, 'info', {'choices': level_choices}),
        ]


def get_test_parser():
    from mozlog.commandline import add_logging_group
    from moztest.resolve import TEST_SUITES
    parser = argparse.ArgumentParser()
    parser.add_argument('what', default=None, nargs='+',
                        help=TEST_HELP.format(', '.join(sorted(TEST_SUITES))))
    parser.add_argument('extra_args', default=None, nargs=argparse.REMAINDER,
                        help="Extra arguments to pass to the underlying test command(s). "
                             "If an underlying command doesn't recognize the argument, it "
                             "will fail.")
    parser.add_argument('--debugger', default=None, action='store',
                        nargs='?', help="Specify a debugger to use.")
    add_logging_group(parser)
    return parser


ADD_TEST_SUPPORTED_SUITES = ['mochitest-chrome', 'mochitest-plain', 'mochitest-browser-chrome',
                             'web-platform-tests-testharness', 'web-platform-tests-reftest',
                             'xpcshell']
ADD_TEST_SUPPORTED_DOCS = ['js', 'html', 'xhtml', 'xul']

SUITE_SYNONYMS = {
    "wpt": "web-platform-tests-testharness",
    "wpt-testharness": "web-platform-tests-testharness",
    "wpt-reftest": "web-platform-tests-reftest"
}

MISSING_ARG = object()


def create_parser_addtest():
    import addtest
    parser = argparse.ArgumentParser()
    parser.add_argument('--suite',
                        choices=sorted(ADD_TEST_SUPPORTED_SUITES + list(SUITE_SYNONYMS.keys())),
                        help='suite for the test. '
                        'If you pass a `test` argument this will be determined '
                        'based on the filename and the folder it is in')
    parser.add_argument('-o', '--overwrite',
                        action='store_true',
                        help='Overwrite an existing file if it exists.')
    parser.add_argument('--doc',
                        choices=ADD_TEST_SUPPORTED_DOCS,
                        help='Document type for the test (if applicable).'
                        'If you pass a `test` argument this will be determined '
                        'based on the filename.')
    parser.add_argument("-e", "--editor", action="store", nargs="?",
                        default=MISSING_ARG, help="Open the created file(s) in an editor; if a "
                        "binary is supplied it will be used otherwise the default editor for "
                        "your environment will be opened")

    for base_suite in addtest.TEST_CREATORS:
        cls = addtest.TEST_CREATORS[base_suite]
        if hasattr(cls, "get_parser"):
            group = parser.add_argument_group(base_suite)
            cls.get_parser(group)

    parser.add_argument('test',
                        nargs='?',
                        help=('Test to create.'))
    return parser


@CommandProvider
class AddTest(MachCommandBase):
    @Command('addtest', category='testing',
             description='Generate tests based on templates',
             parser=create_parser_addtest)
    def addtest(self, suite=None, test=None, doc=None, overwrite=False,
                editor=MISSING_ARG, **kwargs):
        import addtest
        import io
        from moztest.resolve import TEST_SUITES

        if not suite and not test:
            return create_parser_addtest().parse_args(["--help"])

        if suite in SUITE_SYNONYMS:
            suite = SUITE_SYNONYMS[suite]

        if test:
            if not overwrite and os.path.isfile(os.path.abspath(test)):
                print("Error: can't generate a test that already exists:", test)
                return 1

            abs_test = os.path.abspath(test)
            if doc is None:
                doc = self.guess_doc(abs_test)
            if suite is None:
                guessed_suite, err = self.guess_suite(abs_test)
                if err:
                    print(err)
                    return 1
                suite = guessed_suite

        else:
            test = None
            if doc is None:
                doc = "html"

        if not suite:
            print("We couldn't automatically determine a suite. "
                  "Please specify `--suite` with one of the following options:\n{}\n"
                  "If you'd like to add support to a new suite, please file a bug "
                  "blocking https://bugzilla.mozilla.org/show_bug.cgi?id=1540285."
                  .format(ADD_TEST_SUPPORTED_SUITES))
            return 1

        if doc not in ADD_TEST_SUPPORTED_DOCS:
            print("Error: invalid `doc`. Either pass in a test with a valid extension"
                  "({}) or pass in the `doc` argument".format(ADD_TEST_SUPPORTED_DOCS))
            return 1

        creator_cls = addtest.creator_for_suite(suite)

        if creator_cls is None:
            print("Sorry, `addtest` doesn't currently know how to add {}".format(suite))
            return 1

        creator = creator_cls(self.topsrcdir, test, suite, doc, **kwargs)

        creator.check_args()

        paths = []
        added_tests = False
        for path, template in creator:
            if not template:
                continue
            added_tests = True
            if (path):
                paths.append(path)
                print("Adding a test file at {} (suite `{}`)".format(path, suite))

                try:
                    os.makedirs(os.path.dirname(path))
                except OSError:
                    pass

                with io.open(path, "w", newline='\n') as f:
                    f.write(template)
            else:
                # write to stdout if you passed only suite and doc and not a file path
                print(template)

        if not added_tests:
            return 1

        if test:
            creator.update_manifest()

            # Small hack, should really do this better
            if suite.startswith("wpt-"):
                suite = "web-platform-tests"

            mach_command = TEST_SUITES[suite]["mach_command"]
            print('Please make sure to add the new test to your commit. '
                  'You can now run the test with:\n    ./mach {} {}'.format(mach_command, test))

        if editor is not MISSING_ARG:
            if editor is not None:
                editor = editor
            elif "VISUAL" in os.environ:
                editor = os.environ["VISUAL"]
            elif "EDITOR" in os.environ:
                editor = os.environ["EDITOR"]
            else:
                print('Unable to determine editor; please specify a binary')
                editor = None

            proc = None
            if editor:
                import subprocess
                proc = subprocess.Popen("%s %s" % (editor, " ".join(paths)), shell=True)

            if proc:
                proc.wait()

        return 0

    def guess_doc(self, abs_test):
        filename = os.path.basename(abs_test)
        return os.path.splitext(filename)[1].strip(".")

    def guess_suite(self, abs_test):
        # If you pass a abs_test, try to detect the type based on the name
        # and folder. This detection can be skipped if you pass the `type` arg.
        err = None
        guessed_suite = None
        parent = os.path.dirname(abs_test)
        filename = os.path.basename(abs_test)

        has_browser_ini = os.path.isfile(os.path.join(parent, "browser.ini"))
        has_chrome_ini = os.path.isfile(os.path.join(parent, "chrome.ini"))
        has_plain_ini = os.path.isfile(os.path.join(parent, "mochitest.ini"))
        has_xpcshell_ini = os.path.isfile(os.path.join(parent, "xpcshell.ini"))

        in_wpt_folder = abs_test.startswith(
            os.path.abspath(os.path.join("testing", "web-platform")))

        if in_wpt_folder:
            guessed_suite = "web-platform-tests-testharness"
            if "/css/" in abs_test:
                guessed_suite = "web-platform-tests-reftest"
        elif (filename.startswith("test_") and
              has_xpcshell_ini and
              self.guess_doc(abs_test) == "js"):
            guessed_suite = "xpcshell"
        else:
            if filename.startswith("browser_") and has_browser_ini:
                guessed_suite = "mochitest-browser-chrome"
            elif filename.startswith("test_"):
                if has_chrome_ini and has_plain_ini:
                    err = ("Error: directory contains both a chrome.ini and mochitest.ini. "
                           "Please set --suite=mochitest-chrome or --suite=mochitest-plain.")
                elif has_chrome_ini:
                    guessed_suite = "mochitest-chrome"
                elif has_plain_ini:
                    guessed_suite = "mochitest-plain"
        return guessed_suite, err


@CommandProvider
class Test(MachCommandBase):
    @Command('test', category='testing',
             description='Run tests (detects the kind of test and runs it).',
             parser=get_test_parser)
    def test(self, what, extra_args, **log_args):
        """Run tests from names or paths.

        mach test accepts arguments specifying which tests to run. Each argument
        can be:

        * The path to a test file
        * A directory containing tests
        * A test suite name
        * An alias to a test suite name (codes used on TreeHerder)

        If no input is provided, tests will be run based on files changed in
        the local tree. Relevant tests, tags, or flavors are determined by
        IMPACTED_TESTS annotations in moz.build files relevant to the
        changed files.

        When paths or directories are given, they are first resolved to test
        files known to the build system.

        If resolved tests belong to more than one test type/flavor/harness,
        the harness for each relevant type/flavor will be invoked. e.g. if
        you specify a directory with xpcshell and browser chrome mochitests,
        both harnesses will be invoked.
        """
        from mozlog.commandline import setup_logging
        from mozlog.handlers import StreamHandler
        from moztest.resolve import get_suite_definition, TestResolver, TEST_SUITES

        resolver = self._spawn(TestResolver)
        run_suites, run_tests = resolver.resolve_metadata(what)

        if not run_suites and not run_tests:
            print(UNKNOWN_TEST)
            return 1

        if log_args.get('debugger', None):
            import mozdebug
            if not mozdebug.get_debugger_info(log_args.get('debugger')):
                sys.exit(1)
            extra_args_debugger_notation = '='.join([
                    '--debugger',
                    log_args.get('debugger')
                ]).encode('ascii')
            if extra_args:
                extra_args.append(extra_args_debugger_notation)
            else:
                extra_args = [extra_args_debugger_notation]

        # Create shared logger
        format_args = {'level': self._mach_context.settings['test']['level']}
        if not run_suites and len(run_tests) == 1:
            format_args['verbose'] = True
            format_args['compact'] = False

        default_format = self._mach_context.settings['test']['format']
        log = setup_logging('mach-test', log_args, {default_format: sys.stdout}, format_args)
        for handler in log.handlers:
            if isinstance(handler, StreamHandler):
                handler.formatter.inner.summary_on_shutdown = True

        status = None
        for suite_name in run_suites:
            suite = TEST_SUITES[suite_name]
            kwargs = suite['kwargs']
            kwargs['log'] = log

            if 'mach_command' in suite:
                res = self._mach_context.commands.dispatch(
                    suite['mach_command'], self._mach_context,
                    argv=extra_args, **kwargs)
                if res:
                    status = res

        buckets = {}
        for test in run_tests:
            key = (test['flavor'], test.get('subsuite', ''))
            buckets.setdefault(key, []).append(test)

        for (flavor, subsuite), tests in sorted(buckets.items()):
            _, m = get_suite_definition(flavor, subsuite)
            if 'mach_command' not in m:
                substr = '-{}'.format(subsuite) if subsuite else ''
                print(UNKNOWN_FLAVOR % (flavor, substr))
                status = 1
                continue

            kwargs = dict(m['kwargs'])
            kwargs['log'] = log

            res = self._mach_context.commands.dispatch(
                m['mach_command'], self._mach_context,
                argv=extra_args, test_objects=tests, **kwargs)
            if res:
                status = res

        log.shutdown()
        return status


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('cppunittest', category='testing',
             description='Run cpp unit tests (C++ tests).')
    @CommandArgument('--enable-webrender', action='store_true', default=False,
                     dest='enable_webrender',
                     help='Enable the WebRender compositor in Gecko.')
    @CommandArgument('test_files', nargs='*', metavar='N',
                     help='Test to run. Can be specified as one or more files or '
                     'directories, or omitted. If omitted, the entire test suite is '
                     'executed.')
    def run_cppunit_test(self, **params):
        from mozlog import commandline

        log = params.get('log')
        if not log:
            log = commandline.setup_logging("cppunittest",
                                            {},
                                            {"tbpl": sys.stdout})

        # See if we have crash symbols
        symbols_path = os.path.join(self.distdir, 'crashreporter-symbols')
        if not os.path.isdir(symbols_path):
            symbols_path = None

        # If no tests specified, run all tests in main manifest
        tests = params['test_files']
        if len(tests) == 0:
            tests = [os.path.join(self.distdir, 'cppunittests')]
            manifest_path = os.path.join(
                self.topsrcdir, 'testing', 'cppunittest.ini')
        else:
            manifest_path = None

        utility_path = self.bindir

        if conditions.is_android(self):
            from mozrunner.devices.android_device import (verify_android_device, InstallIntent)
            verify_android_device(self, install=InstallIntent.NO)
            return self.run_android_test(tests, symbols_path, manifest_path, log)

        return self.run_desktop_test(tests, symbols_path, manifest_path,
                                     utility_path, log)

    def run_desktop_test(self, tests, symbols_path, manifest_path,
                         utility_path, log):
        import runcppunittests as cppunittests
        from mozlog import commandline

        parser = cppunittests.CPPUnittestOptions()
        commandline.add_logging_group(parser)
        options, args = parser.parse_args()

        options.symbols_path = symbols_path
        options.manifest_path = manifest_path
        options.utility_path = utility_path
        options.xre_path = self.bindir

        try:
            result = cppunittests.run_test_harness(options, tests)
        except Exception as e:
            log.error("Caught exception running cpp unit tests: %s" % str(e))
            result = False
            raise

        return 0 if result else 1

    def run_android_test(self, tests, symbols_path, manifest_path, log):
        import remotecppunittests as remotecppunittests
        from mozlog import commandline

        parser = remotecppunittests.RemoteCPPUnittestOptions()
        commandline.add_logging_group(parser)
        options, args = parser.parse_args()

        if not options.adb_path:
            from mozrunner.devices.android_device import get_adb_path
            options.adb_path = get_adb_path(self)
        options.symbols_path = symbols_path
        options.manifest_path = manifest_path
        options.xre_path = self.bindir
        options.local_lib = self.bindir.replace('bin', 'fennec')
        for file in os.listdir(os.path.join(self.topobjdir, "dist")):
            if file.endswith(".apk") and file.startswith("fennec"):
                options.local_apk = os.path.join(self.topobjdir, "dist", file)
                log.info("using APK: " + options.local_apk)
                break

        try:
            result = remotecppunittests.run_test_harness(options, tests)
        except Exception as e:
            log.error("Caught exception running cpp unit tests: %s" % str(e))
            result = False
            raise

        return 0 if result else 1


def executable_name(name):
    return name + '.exe' if sys.platform.startswith('win') else name


@CommandProvider
class SpiderMonkeyTests(MachCommandBase):
    @Command('jstests', category='testing',
             description='Run SpiderMonkey JS tests in the JS shell.')
    @CommandArgument('--shell', help='The shell to be used')
    @CommandArgument('params', nargs=argparse.REMAINDER,
                     help="Extra arguments to pass down to the test harness.")
    def run_jstests(self, shell, params):
        import subprocess

        self.virtualenv_manager.ensure()
        python = self.virtualenv_manager.python_path

        js = shell or os.path.join(self.bindir, executable_name('js'))
        jstest_cmd = [
            python,
            os.path.join(self.topsrcdir, 'js', 'src', 'tests', 'jstests.py'),
            js,
        ] + params

        return subprocess.call(jstest_cmd)

    @Command('jit-test', category='testing',
             description='Run SpiderMonkey jit-tests in the JS shell.')
    @CommandArgument('--shell', help='The shell to be used')
    @CommandArgument('params', nargs=argparse.REMAINDER,
                     help="Extra arguments to pass down to the test harness.")
    def run_jittests(self, shell, params):
        import subprocess

        self.virtualenv_manager.ensure()
        python = self.virtualenv_manager.python_path

        js = shell or os.path.join(self.bindir, executable_name('js'))
        jittest_cmd = [
            python,
            os.path.join(self.topsrcdir, 'js', 'src', 'jit-test', 'jit_test.py'),
            js,
        ] + params

        return subprocess.call(jittest_cmd)

    @Command('jsapi-tests', category='testing',
             description='Run SpiderMonkey JSAPI tests.')
    @CommandArgument('test_name', nargs='?', metavar='N',
                     help='Test to run. Can be a prefix or omitted. If '
                     'omitted, the entire test suite is executed.')
    def run_jsapitests(self, test_name=None):
        import subprocess

        jsapi_tests_cmd = [
            os.path.join(self.bindir, executable_name('jsapi-tests'))
        ]
        if test_name:
            jsapi_tests_cmd.append(test_name)

        test_env = os.environ.copy()
        test_env["TOPSRCDIR"] = self.topsrcdir

        return subprocess.call(jsapi_tests_cmd, env=test_env)

    def run_check_js_msg(self):
        import subprocess

        self.virtualenv_manager.ensure()
        python = self.virtualenv_manager.python_path

        check_cmd = [
            python,
            os.path.join(self.topsrcdir, 'config', 'check_js_msg_encoding.py')
        ]

        return subprocess.call(check_cmd)

    @Command('check-spidermonkey', category='testing',
             description='Run SpiderMonkey tests (JavaScript engine).')
    @CommandArgument('--valgrind', action='store_true',
                     help='Run jit-test suite with valgrind flag')
    def run_checkspidermonkey(self, valgrind=False):
        print('Running jit-tests')
        jittest_args = [
            '--no-slow',
            '--jitflags=all',
        ]
        if valgrind:
            jittest_args.append('--valgrind')
        jittest_result = self.run_jittests(shell=None, params=jittest_args)

        print('running jstests')
        jstest_result = self.run_jstests(shell=None, params=[])

        print('running jsapi-tests')
        jsapi_tests_result = self.run_jsapitests(test_name=None)

        print('running check-js-msg-encoding')
        check_js_msg_result = self.run_check_js_msg()

        return jittest_result and jstest_result and jsapi_tests_result and check_js_msg_result


def get_jsshell_parser():
    from jsshell.benchmark import get_parser
    return get_parser()


@CommandProvider
class JsShellTests(MachCommandBase):
    @Command('jsshell-bench', category='testing',
             parser=get_jsshell_parser,
             description="Run benchmarks in the SpiderMonkey JS shell.")
    def run_jsshelltests(self, **kwargs):
        self._activate_virtualenv()
        from jsshell import benchmark
        return benchmark.run(**kwargs)


@CommandProvider
class CramTest(MachCommandBase):
    @Command('cramtest', category='testing',
             description="Mercurial style .t tests for command line applications.")
    @CommandArgument('test_paths', nargs='*', metavar='N',
                     help="Test paths to run. Each path can be a test file or directory. "
                          "If omitted, the entire suite will be run.")
    @CommandArgument('cram_args', nargs=argparse.REMAINDER,
                     help="Extra arguments to pass down to the cram binary. See "
                          "'./mach python -m cram -- -h' for a list of available options.")
    def cramtest(self, cram_args=None, test_paths=None, test_objects=None):
        self._activate_virtualenv()
        import mozinfo
        from manifestparser import TestManifest

        if test_objects is None:
            from moztest.resolve import TestResolver
            resolver = self._spawn(TestResolver)
            if test_paths:
                # If we were given test paths, try to find tests matching them.
                test_objects = resolver.resolve_tests(paths=test_paths, flavor='cram')
            else:
                # Otherwise just run everything in CRAMTEST_MANIFESTS
                test_objects = resolver.resolve_tests(flavor='cram')

        if not test_objects:
            message = 'No tests were collected, check spelling of the test paths.'
            self.log(logging.WARN, 'cramtest', {}, message)
            return 1

        mp = TestManifest()
        mp.tests.extend(test_objects)
        tests = mp.active_tests(disabled=False, **mozinfo.info)

        python = self.virtualenv_manager.python_path
        cmd = [python, '-m', 'cram'] + cram_args + [t['relpath'] for t in tests]
        return subprocess.call(cmd, cwd=self.topsrcdir)


@CommandProvider
class TestInfoCommand(MachCommandBase):
    from datetime import date, timedelta

    @Command('test-info', category='testing',
             description='Display historical test results.')
    def test_info(self):
        """
           All functions implemented as subcommands.
        """

    @SubCommand('test-info', 'tests',
                description='Display historical test result summary for named tests.')
    @CommandArgument('test_names', nargs=argparse.REMAINDER,
                     help='Test(s) of interest.')
    @CommandArgument('--branches',
                     default='mozilla-central,autoland',
                     help='Report for named branches '
                          '(default: mozilla-central,autoland)')
    @CommandArgument('--start',
                     default=(date.today() - timedelta(7)
                              ).strftime("%Y-%m-%d"),
                     help='Start date (YYYY-MM-DD)')
    @CommandArgument('--end',
                     default=date.today().strftime("%Y-%m-%d"),
                     help='End date (YYYY-MM-DD)')
    @CommandArgument('--show-info', action='store_true',
                     help='Retrieve and display general test information.')
    @CommandArgument('--show-results', action='store_true',
                     help='Retrieve and display ActiveData test result summary.')
    @CommandArgument('--show-durations', action='store_true',
                     help='Retrieve and display ActiveData test duration summary.')
    @CommandArgument('--show-tasks', action='store_true',
                     help='Retrieve and display ActiveData test task names.')
    @CommandArgument('--show-bugs', action='store_true',
                     help='Retrieve and display related Bugzilla bugs.')
    @CommandArgument('--verbose', action='store_true',
                     help='Enable debug logging.')
    def test_info_tests(self, test_names, branches, start, end,
                        show_info, show_results, show_durations, show_tasks, show_bugs,
                        verbose):
        import testinfo

        ti = testinfo.TestInfoTests(verbose)
        ti.report(test_names, branches, start, end,
                  show_info, show_results, show_durations, show_tasks, show_bugs)

    @SubCommand('test-info', 'long-tasks',
                description='Find tasks approaching their taskcluster max-run-time.')
    @CommandArgument('--branches',
                     default='mozilla-central,autoland',
                     help='Report for named branches '
                          '(default: mozilla-central,autoland)')
    @CommandArgument('--start',
                     default=(date.today() - timedelta(7)
                              ).strftime("%Y-%m-%d"),
                     help='Start date (YYYY-MM-DD)')
    @CommandArgument('--end',
                     default=date.today().strftime("%Y-%m-%d"),
                     help='End date (YYYY-MM-DD)')
    @CommandArgument('--max-threshold-pct',
                     default=90.0,
                     help='Count tasks exceeding this percentage of max-run-time.')
    @CommandArgument('--filter-threshold-pct',
                     default=0.5,
                     help='Report tasks exceeding this percentage of long tasks.')
    @CommandArgument('--verbose', action='store_true',
                     help='Enable debug logging.')
    def report_long_running_tasks(self, branches, start, end,
                                  max_threshold_pct, filter_threshold_pct, verbose):
        import testinfo

        max_threshold_pct = float(max_threshold_pct)
        filter_threshold_pct = float(filter_threshold_pct)

        ti = testinfo.TestInfoLongRunningTasks(verbose)
        ti.report(branches, start, end, max_threshold_pct, filter_threshold_pct)

    @SubCommand('test-info', 'report',
                description='Generate a json report of test manifests and/or tests '
                            'categorized by Bugzilla component and optionally filtered '
                            'by path, component, and/or manifest annotations.')
    @CommandArgument('--components', default=None,
                     help='Comma-separated list of Bugzilla components.'
                          ' eg. Testing::General,Core::WebVR')
    @CommandArgument('--flavor',
                     help='Limit results to tests of the specified flavor (eg. "xpcshell").')
    @CommandArgument('--subsuite',
                     help='Limit results to tests of the specified subsuite (eg. "devtools").')
    @CommandArgument('paths', nargs=argparse.REMAINDER,
                     help='File system paths of interest.')
    @CommandArgument('--show-manifests', action='store_true',
                     help='Include test manifests in report.')
    @CommandArgument('--show-tests', action='store_true',
                     help='Include individual tests in report.')
    @CommandArgument('--show-summary', action='store_true',
                     help='Include summary in report.')
    @CommandArgument('--show-annotations', action='store_true',
                     help='Include list of manifest annotation conditions in report.')
    @CommandArgument('--show-activedata', action='store_true',
                     help='Include additional data from ActiveData, like run times and counts.')
    @CommandArgument('--filter-values',
                     help='Comma-separated list of value regular expressions to filter on; '
                          'displayed tests contain all specified values.')
    @CommandArgument('--filter-keys',
                     help='Comma-separated list of test keys to filter on, '
                          'like "skip-if"; only these fields will be searched '
                          'for filter-values.')
    @CommandArgument('--no-component-report', action='store_false',
                     dest="show_components", default=True,
                     help='Do not categorize by bugzilla component.')
    @CommandArgument('--output-file',
                     help='Path to report file.')
    @CommandArgument('--branches',
                     default='mozilla-central,autoland',
                     help='Query ActiveData for named branches '
                          '(default: mozilla-central,autoland)')
    @CommandArgument('--days',
                     type=int,
                     default=7,
                     help='Query ActiveData for specified number of days')
    @CommandArgument('--verbose', action='store_true',
                     help='Enable debug logging.')
    def test_report(self, components, flavor, subsuite, paths,
                    show_manifests, show_tests, show_summary, show_annotations,
                    show_activedata,
                    filter_values, filter_keys, show_components, output_file,
                    branches, days, verbose):
        import testinfo
        from mozbuild.build_commands import Build

        try:
            self.config_environment
        except BuildEnvironmentNotFoundException:
            print("Looks like configure has not run yet, running it now...")
            builder = Build(self._mach_context)
            builder.configure()

        ti = testinfo.TestInfoReport(verbose)
        ti.report(components, flavor, subsuite, paths,
                  show_manifests, show_tests, show_summary, show_annotations,
                  show_activedata,
                  filter_values, filter_keys, show_components, output_file,
                  branches, days)

    @SubCommand('test-info', 'report-diff',
                description='Compare two reports generated by "test-info reports".')
    @CommandArgument('--before', default=None,
                     help='The first (earlier) report file; path to local file or url.')
    @CommandArgument('--after',
                     help='The second (later) report file; path to local file or url.')
    @CommandArgument('--output-file',
                     help='Path to report file to be written. If not specified, report'
                          'will be written to standard output.')
    @CommandArgument('--verbose', action='store_true',
                     help='Enable debug logging.')
    def test_report_diff(self, before, after, output_file, verbose):
        import testinfo
        ti = testinfo.TestInfoReport(verbose)
        ti.report_diff(before, after, output_file)


@CommandProvider
class RustTests(MachCommandBase):
    @Command('rusttests', category='testing',
             conditions=[conditions.is_non_artifact_build],
             description="Run rust unit tests (via cargo test).")
    def run_rusttests(self, **kwargs):
        return self._mach_context.commands.dispatch('build', self._mach_context,
                                                    what=['pre-export',
                                                          'export',
                                                          'recurse_rusttests'])


@CommandProvider
class TestFluentMigration(MachCommandBase):
    @Command('fluent-migration-test', category='testing',
             description="Test Fluent migration recipes.")
    @CommandArgument('test_paths', nargs='*', metavar='N',
                     help="Recipe paths to test.")
    def run_migration_tests(self, test_paths=None, **kwargs):
        if not test_paths:
            test_paths = []
        self._activate_virtualenv()
        from test_fluent_migrations import fmt
        rv = 0
        with_context = []
        for to_test in test_paths:
            try:
                context = fmt.inspect_migration(to_test)
                for issue in context['issues']:
                    self.log(logging.ERROR, 'fluent-migration-test', {
                        'error': issue['msg'],
                        'file': to_test,
                    }, 'ERROR in {file}: {error}')
                if context['issues']:
                    continue
                with_context.append({
                    'to_test': to_test,
                    'references': context['references'],
                })
            except Exception as e:
                self.log(logging.ERROR, 'fluent-migration-test', {
                    'error': str(e),
                    'file': to_test
                }, 'ERROR in {file}: {error}')
                rv |= 1
        obj_dir = fmt.prepare_object_dir(self)
        for context in with_context:
            rv |= fmt.test_migration(self, obj_dir, **context)
        return rv

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import errno
import json
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
                        choices=sorted(ADD_TEST_SUPPORTED_SUITES + SUITE_SYNONYMS.keys()),
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

                with open(path, "w") as f:
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
            from mozrunner.devices.android_device import verify_android_device
            verify_android_device(self, install=False)
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
class CheckSpiderMonkeyCommand(MachCommandBase):
    @Command('jstests', category='testing',
             description='Run SpiderMonkey JS tests in the JavaScript shell.')
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
            '--jitflags=jstests',
        ] + params
        return subprocess.call(jstest_cmd)

    @Command('check-spidermonkey', category='testing',
             description='Run SpiderMonkey tests (JavaScript engine).')
    @CommandArgument('--valgrind', action='store_true',
                     help='Run jit-test suite with valgrind flag')
    def run_checkspidermonkey(self, **params):
        import subprocess

        self.virtualenv_manager.ensure()
        python = self.virtualenv_manager.python_path

        js = os.path.join(self.bindir, executable_name('js'))

        print('Running jit-tests')
        jittest_cmd = [
            python,
            os.path.join(self.topsrcdir, 'js', 'src',
                         'jit-test', 'jit_test.py'),
            js,
            '--no-slow',
            '--jitflags=all',
        ]
        if params['valgrind']:
            jittest_cmd.append('--valgrind')

        jittest_result = subprocess.call(jittest_cmd)

        print('running jstests')
        jstest_result = self.run_jstests(js, [])

        print('running jsapi-tests')
        jsapi_tests_cmd = [os.path.join(
            self.bindir, executable_name('jsapi-tests'))]
        jsapi_tests_result = subprocess.call(jsapi_tests_cmd)

        print('running check-js-msg-encoding')
        check_js_msg_cmd = [python, os.path.join(
            self.topsrcdir, 'config', 'check_js_msg_encoding.py')]
        check_js_msg_result = subprocess.call(
            check_js_msg_cmd, cwd=self.topsrcdir)

        all_passed = jittest_result and jstest_result and jsapi_tests_result and \
            check_js_msg_result

        return all_passed


def has_js_binary(binary):
    def has_binary(cls):
        try:
            name = binary + cls.substs['BIN_SUFFIX']
        except BuildEnvironmentNotFoundException:
            return False

        path = os.path.join(cls.topobjdir, 'dist', 'bin', name)

        has_binary.__doc__ = """
`{}` not found in <objdir>/dist/bin. Make sure you aren't using an artifact build
and try rebuilding with `ac_add_options --enable-js-shell`.
""".format(name).lstrip()

        return os.path.isfile(path)
    return has_binary


@CommandProvider
class JsapiTestsCommand(MachCommandBase):
    @Command('jsapi-tests', category='testing',
             conditions=[has_js_binary('jsapi-tests')],
             description='Run jsapi tests (JavaScript engine).')
    @CommandArgument('test_name', nargs='?', metavar='N',
                     help='Test to run. Can be a prefix or omitted. If omitted, the entire '
                     'test suite is executed.')
    def run_jsapitests(self, **params):
        import subprocess

        print('running jsapi-tests')
        jsapi_tests_cmd = [os.path.join(
            self.bindir, executable_name('jsapi-tests'))]
        if params['test_name']:
            jsapi_tests_cmd.append(params['test_name'])

        jsapi_tests_result = subprocess.call(jsapi_tests_cmd)

        return jsapi_tests_result


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
             description='Display historical test result summary.')
    @CommandArgument('test_names', nargs=argparse.REMAINDER,
                     help='Test(s) of interest.')
    @CommandArgument('--branches',
                     default='mozilla-central,mozilla-inbound,autoland',
                     help='Report for named branches '
                          '(default: mozilla-central,mozilla-inbound,autoland)')
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
    def test_info(self, **params):
        from mozbuild.base import MozbuildObject
        from mozfile import which

        self.branches = params['branches']
        self.start = params['start']
        self.end = params['end']
        self.show_info = params['show_info']
        self.show_results = params['show_results']
        self.show_durations = params['show_durations']
        self.show_tasks = params['show_tasks']
        self.show_bugs = params['show_bugs']
        self.verbose = params['verbose']

        if (not self.show_info and
            not self.show_results and
            not self.show_durations and
            not self.show_tasks and
                not self.show_bugs):
            # by default, show everything
            self.show_info = True
            self.show_results = True
            self.show_durations = True
            self.show_tasks = True
            self.show_bugs = True

        here = os.path.abspath(os.path.dirname(__file__))
        build_obj = MozbuildObject.from_environment(cwd=here)

        self._hg = None
        if conditions.is_hg(build_obj):
            self._hg = which('hg')
            if not self._hg:
                raise OSError(errno.ENOENT, "Could not find 'hg' on PATH.")

        self._git = None
        if conditions.is_git(build_obj):
            self._git = which('git')
            if not self._git:
                raise OSError(errno.ENOENT, "Could not find 'git' on PATH.")

        for test_name in params['test_names']:
            print("===== %s =====" % test_name)
            self.test_name = test_name
            if len(self.test_name) < 6:
                print("'%s' is too short for a test name!" % self.test_name)
                continue
            self.set_test_name()
            if self.show_results:
                self.report_test_results()
            if self.show_durations:
                self.report_test_durations()
            if self.show_tasks:
                self.report_test_tasks()
            if self.show_bugs:
                self.report_bugs()

    def find_in_hg_or_git(self, test_name):
        if self._hg:
            cmd = [self._hg, 'files', '-I', test_name]
        elif self._git:
            cmd = [self._git, 'ls-files', test_name]
        else:
            return None
        try:
            out = subprocess.check_output(cmd).splitlines()
        except subprocess.CalledProcessError:
            out = None
        return out

    def set_test_name(self):
        # Generating a unified report for a specific test is complicated
        # by differences in the test name used in various data sources.
        # Consider:
        #   - It is often convenient to request a report based only on
        #     a short file name, rather than the full path;
        #   - Bugs may be filed in bugzilla against a simple, short test
        #     name or the full path to the test;
        #   - In ActiveData, the full path is usually used, but sometimes
        #     also includes additional path components outside of the
        #     mercurial repo (common for reftests).
        # This function attempts to find appropriate names for different
        # queries based on the specified test name.

        import posixpath
        import re

        # full_test_name is full path to file in hg (or git)
        self.full_test_name = None
        out = self.find_in_hg_or_git(self.test_name)
        if out and len(out) == 1:
            self.full_test_name = out[0]
        elif out and len(out) > 1:
            print("Ambiguous test name specified. Found:")
            for line in out:
                print(line)
        else:
            out = self.find_in_hg_or_git('**/%s*' % self.test_name)
            if out and len(out) == 1:
                self.full_test_name = out[0]
            elif out and len(out) > 1:
                print("Ambiguous test name. Found:")
                for line in out:
                    print(line)
        if self.full_test_name:
            self.full_test_name.replace(os.sep, posixpath.sep)
            print("Found %s in source control." % self.full_test_name)
        else:
            print("Unable to validate test name '%s'!" % self.test_name)
            self.full_test_name = self.test_name

        # search for full_test_name in test manifests
        from moztest.resolve import TestResolver
        resolver = self._spawn(TestResolver)
        relpath = self._wrap_path_argument(self.full_test_name).relpath()
        tests = list(resolver.resolve_tests(paths=[relpath]))
        if len(tests) == 1:
            relpath = self._wrap_path_argument(tests[0]['manifest']).relpath()
            print("%s found in manifest %s" % (self.full_test_name, relpath))
            if tests[0].get('flavor'):
                print("  flavor: %s" % tests[0]['flavor'])
            if tests[0].get('skip-if'):
                print("  skip-if: %s" % tests[0]['skip-if'])
            if tests[0].get('fail-if'):
                print("  fail-if: %s" % tests[0]['fail-if'])
        elif len(tests) == 0:
            print("%s not found in any test manifest!" % self.full_test_name)
        else:
            print("%s found in more than one manifest!" % self.full_test_name)

        # short_name is full_test_name without path
        self.short_name = None
        name_idx = self.full_test_name.rfind('/')
        if name_idx > 0:
            self.short_name = self.full_test_name[name_idx + 1:]

        # robo_name is short_name without ".java" - for robocop
        self.robo_name = None
        if self.short_name:
            robo_idx = self.short_name.rfind('.java')
            if robo_idx > 0:
                self.robo_name = self.short_name[:robo_idx]
            if self.short_name == self.test_name:
                self.short_name = None

        if not (self.show_results or self.show_durations or self.show_tasks):
            # no need to determine ActiveData name if not querying
            return

        # activedata_test_name is name in ActiveData
        self.activedata_test_name = None
        simple_names = [
            self.full_test_name,
            self.test_name,
            self.short_name,
            self.robo_name
        ]
        simple_names = [x for x in simple_names if x]
        searches = [
            {"in": {"result.test": simple_names}},
        ]
        regex_names = [".*%s.*" % re.escape(x) for x in simple_names if x]
        for r in regex_names:
            searches.append({"regexp": {"result.test": r}})
        query = {
            "from": "unittest",
            "format": "list",
            "limit": 10,
            "groupby": ["result.test"],
            "where": {"and": [
                {"or": searches},
                {"in": {"build.branch": self.branches.split(',')}},
                {"gt": {"run.timestamp": {"date": self.start}}},
                {"lt": {"run.timestamp": {"date": self.end}}}
            ]}
        }
        print("Querying ActiveData...")  # Following query can take a long time
        data = self.submit(query)
        if data and len(data) > 0:
            self.activedata_test_name = [
                d['result']['test']
                for p in simple_names + regex_names
                for d in data
                if re.match(p + "$", d['result']['test'])
            ][0]  # first match is best match
        if self.activedata_test_name:
            print("Found records matching '%s' in ActiveData." %
                  self.activedata_test_name)
        else:
            print("Unable to find matching records in ActiveData; using %s!" %
                  self.test_name)
            self.activedata_test_name = self.test_name

    def get_platform(self, record):
        if 'platform' in record['build']:
            platform = record['build']['platform']
        else:
            platform = "-"
        tp = record['build']['type']
        if type(tp) is list:
            tp = "-".join(tp)
        e10s = ""
        if 'run' in record and 'type' in record['run'] and 'e10s' in str(record['run']['type']):
            e10s = "-e10s"
        if 'run' in record and 'type' in record['run'] and 'fis' in str(record['run']['type']):
            # fission implies e10s - keep the label simple
            e10s = "-fis"
        return "%s/%s%s:" % (platform, tp, e10s)

    def submit(self, query):
        import requests
        import datetime
        if self.verbose:
            print(datetime.datetime.now())
            print(json.dumps(query))
        response = requests.post("http://activedata.allizom.org/query",
                                 data=json.dumps(query),
                                 stream=True)
        if self.verbose:
            print(datetime.datetime.now())
            print(response)
        response.raise_for_status()
        data = response.json()["data"]
        return data

    def report_test_results(self):
        # Report test pass/fail summary from ActiveData
        query = {
            "from": "unittest",
            "format": "list",
            "limit": 100,
            "groupby": ["build.platform", "build.type", "run.type"],
            "select": [
                {"aggregate": "count"},
                {
                    "name": "failures",
                    "value": {"case": [
                        {"when": {"eq": {"result.ok": "F"}}, "then": 1}
                    ]},
                    "aggregate": "sum",
                    "default": 0
                },
                {
                    "name": "skips",
                    "value": {"case": [
                        {"when": {"eq": {"result.status": "SKIP"}}, "then": 1}
                    ]},
                    "aggregate": "sum",
                    "default": 0
                }
            ],
            "where": {"and": [
                {"eq": {"result.test": self.activedata_test_name}},
                {"in": {"build.branch": self.branches.split(',')}},
                {"gt": {"run.timestamp": {"date": self.start}}},
                {"lt": {"run.timestamp": {"date": self.end}}}
            ]}
        }
        print("\nTest results for %s on %s between %s and %s" %
              (self.activedata_test_name, self.branches, self.start, self.end))
        data = self.submit(query)
        if data and len(data) > 0:
            data.sort(key=self.get_platform)
            worst_rate = 0.0
            worst_platform = None
            total_runs = 0
            total_failures = 0
            for record in data:
                platform = self.get_platform(record)
                if platform.startswith("-"):
                    continue
                runs = record['count']
                total_runs = total_runs + runs
                failures = record.get('failures', 0)
                skips = record.get('skips', 0)
                total_failures = total_failures + failures
                rate = (float)(failures) / runs
                if rate >= worst_rate:
                    worst_rate = rate
                    worst_platform = platform
                    worst_failures = failures
                    worst_runs = runs
                print("%-40s %6d failures (%6d skipped) in %6d runs" % (
                    platform, failures, skips, runs))
            print("\nTotal: %d failures in %d runs or %.3f failures/run" %
                  (total_failures, total_runs, (float)(total_failures) / total_runs))
            if worst_failures > 0:
                print("Worst rate on %s %d failures in %d runs or %.3f failures/run" %
                      (worst_platform, worst_failures, worst_runs, worst_rate))
        else:
            print("No test result data found.")

    def report_test_durations(self):
        # Report test durations summary from ActiveData
        query = {
            "from": "unittest",
            "format": "list",
            "limit": 100,
            "groupby": ["build.platform", "build.type", "run.type"],
            "select": [
                {"value": "result.duration",
                    "aggregate": "average", "name": "average"},
                {"value": "result.duration", "aggregate": "min", "name": "min"},
                {"value": "result.duration", "aggregate": "max", "name": "max"},
                {"aggregate": "count"}
            ],
            "where": {"and": [
                {"eq": {"result.ok": "T"}},
                {"eq": {"result.test": self.activedata_test_name}},
                {"in": {"build.branch": self.branches.split(',')}},
                {"gt": {"run.timestamp": {"date": self.start}}},
                {"lt": {"run.timestamp": {"date": self.end}}}
            ]}
        }
        data = self.submit(query)
        print("\nTest durations for %s on %s between %s and %s" %
              (self.activedata_test_name, self.branches, self.start, self.end))
        if data and len(data) > 0:
            data.sort(key=self.get_platform)
            for record in data:
                platform = self.get_platform(record)
                if platform.startswith("-"):
                    continue
                print("%-40s %6.2f s (%.2f s - %.2f s over %d runs)" % (
                    platform, record['average'], record['min'],
                    record['max'], record['count']))
        else:
            print("No test durations found.")

    def report_test_tasks(self):
        # Report test tasks summary from ActiveData
        query = {
            "from": "unittest",
            "format": "list",
            "limit": 1000,
            "select": ["build.platform", "build.type", "run.type", "run.name"],
            "where": {"and": [
                {"eq": {"result.test": self.activedata_test_name}},
                {"in": {"build.branch": self.branches.split(',')}},
                {"gt": {"run.timestamp": {"date": self.start}}},
                {"lt": {"run.timestamp": {"date": self.end}}}
            ]}
        }
        data = self.submit(query)
        print("\nTest tasks for %s on %s between %s and %s" %
              (self.activedata_test_name, self.branches, self.start, self.end))
        if data and len(data) > 0:
            data.sort(key=self.get_platform)
            consolidated = {}
            for record in data:
                platform = self.get_platform(record)
                if platform not in consolidated:
                    consolidated[platform] = {}
                if record['run']['name'] in consolidated[platform]:
                    consolidated[platform][record['run']['name']] += 1
                else:
                    consolidated[platform][record['run']['name']] = 1
            for key in sorted(consolidated.keys()):
                tasks = ""
                for task in consolidated[key].keys():
                    if tasks:
                        tasks += "\n%-40s " % ""
                    tasks += task
                    tasks += " in %d runs" % consolidated[key][task]
                print("%-40s %s" % (key, tasks))
        else:
            print("No test tasks found.")

    def report_bugs(self):
        # Report open bugs matching test name
        import requests
        search = self.full_test_name
        if self.test_name:
            search = '%s,%s' % (search, self.test_name)
        if self.short_name:
            search = '%s,%s' % (search, self.short_name)
        if self.robo_name:
            search = '%s,%s' % (search, self.robo_name)
        payload = {'quicksearch': search,
                   'include_fields': 'id,summary'}
        response = requests.get('https://bugzilla.mozilla.org/rest/bug',
                                payload)
        response.raise_for_status()
        json_response = response.json()
        print("\nBugzilla quick search for '%s':" % search)
        if 'bugs' in json_response:
            for bug in json_response['bugs']:
                print("Bug %s: %s" % (bug['id'], bug['summary']))
        else:
            print("No bugs found.")

    @SubCommand('test-info', 'long-tasks',
                description='Find tasks approaching their taskcluster max-run-time.')
    @CommandArgument('--branches',
                     default='mozilla-central,mozilla-inbound,autoland',
                     help='Report for named branches '
                          '(default: mozilla-central,mozilla-inbound,autoland)')
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
    def report_long_running_tasks(self, **params):
        def get_long_running_ratio(record):
            count = record['count']
            tasks_gt_pct = record['tasks_gt_pct']
            return count / tasks_gt_pct

        branches = params['branches']
        start = params['start']
        end = params['end']
        self.verbose = params['verbose']
        threshold_pct = float(params['max_threshold_pct'])
        filter_threshold_pct = float(params['filter_threshold_pct'])

        # Search test durations in ActiveData for long-running tests
        query = {
            "from": "task",
            "format": "list",
            "groupby": ["run.name"],
            "limit": 1000,
            "select": [
                {
                    "value": "task.maxRunTime",
                    "aggregate": "median",
                    "name": "max_run_time"
                },
                {
                    "aggregate": "count"
                },
                {
                    "value": {
                        "when": {
                            "gt": [
                                {
                                    "div": ["action.duration", "task.maxRunTime"]
                                }, threshold_pct/100.0
                            ]
                        },
                        "then": 1
                    },
                    "aggregate": "sum",
                    "name": "tasks_gt_pct"
                },
            ],
            "where": {"and": [
                {"in": {"build.branch": branches.split(',')}},
                {"gt": {"task.run.start_time": {"date": start}}},
                {"lte": {"task.run.start_time": {"date": end}}},
                {"eq": {"task.state": "completed"}},
            ]}
        }
        data = self.submit(query)
        print("\nTasks nearing their max-run-time on %s between %s and %s" %
              (branches, start, end))
        if data and len(data) > 0:
            filtered = []
            for record in data:
                if 'tasks_gt_pct' in record:
                    count = record['count']
                    tasks_gt_pct = record['tasks_gt_pct']
                    if float(tasks_gt_pct) / count > filter_threshold_pct / 100.0:
                        filtered.append(record)
            filtered.sort(key=get_long_running_ratio)
            if not filtered:
                print("No long running tasks found.")
            for record in filtered:
                name = record['run']['name']
                count = record['count']
                max_run_time = record['max_run_time']
                tasks_gt_pct = record['tasks_gt_pct']
                print("%-55s: %d of %d runs (%.1f%%) exceeded %d%% of max-run-time (%d s)" %
                      (name, tasks_gt_pct, count, tasks_gt_pct * 100 / count,
                       threshold_pct, max_run_time))
        else:
            print("No tasks found.")

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
    def test_report(self, components, flavor, subsuite, paths,
                    show_manifests, show_tests, show_summary,
                    filter_values, filter_keys, show_components, output_file):
        import mozpack.path as mozpath
        import re
        from moztest.resolve import TestResolver

        def matches_filters(test):
            '''
               Return True if all of the requested filter_values are found in this test;
               if filter_keys are specified, restrict search to those test keys.
            '''
            for value in filter_values:
                value_found = False
                for key in test:
                    if not filter_keys or key in filter_keys:
                        if re.search(value, test[key]):
                            value_found = True
                            break
                if not value_found:
                    return False
            return True

        # Ensure useful report by default
        if not show_manifests and not show_tests and not show_summary:
            show_manifests = True
            show_summary = True

        by_component = {}
        if components:
            components = components.split(',')
        if filter_keys:
            filter_keys = filter_keys.split(',')
        if filter_values:
            filter_values = filter_values.split(',')
        else:
            filter_values = []

        print("Finding tests...")
        resolver = self._spawn(TestResolver)
        tests = list(resolver.resolve_tests(paths=paths, flavor=flavor,
                                            subsuite=subsuite))

        manifest_paths = set()
        for t in tests:
            manifest_paths.add(t['manifest'])
        manifest_count = len(manifest_paths)
        print("Resolver found {} tests, {} manifests".format(len(tests), manifest_count))

        if show_manifests:
            by_component['manifests'] = {}
            manifest_paths = list(manifest_paths)
            manifest_paths.sort()
            for manifest_path in manifest_paths:
                relpath = mozpath.relpath(manifest_path, self.topsrcdir)
                print("  {}".format(relpath))
                if mozpath.commonprefix((manifest_path, self.topsrcdir)) != self.topsrcdir:
                    continue
                reader = self.mozbuild_reader(config_mode='empty')
                manifest_info = None
                for info_path, info in reader.files_info([manifest_path]).items():
                    bug_component = info.get('BUG_COMPONENT')
                    key = "{}::{}".format(bug_component.product, bug_component.component)
                    if (info_path == relpath) and ((not components) or (key in components)):
                        manifest_info = {
                            'manifest': relpath,
                            'tests': 0,
                            'skipped': 0
                        }
                        rkey = key if show_components else 'all'
                        if rkey in by_component['manifests']:
                            by_component['manifests'][rkey].append(manifest_info)
                        else:
                            by_component['manifests'][rkey] = [manifest_info]
                        break
                if manifest_info:
                    for t in tests:
                        if t['manifest'] == manifest_path:
                            manifest_info['tests'] += 1
                            if t.get('skip-if'):
                                manifest_info['skipped'] += 1
            for key in by_component['manifests']:
                by_component['manifests'][key].sort()

        if show_tests:
            by_component['tests'] = {}

        if show_tests or show_summary:
            test_count = 0
            failed_count = 0
            skipped_count = 0
            component_set = set()
            for t in tests:
                reader = self.mozbuild_reader(config_mode='empty')
                if not matches_filters(t):
                    continue
                test_count += 1
                relpath = t.get('srcdir_relpath')
                for info_path, info in reader.files_info([relpath]).items():
                    bug_component = info.get('BUG_COMPONENT')
                    key = "{}::{}".format(bug_component.product, bug_component.component)
                    if (info_path == relpath) and ((not components) or (key in components)):
                        component_set.add(key)
                        test_info = {'test': relpath}
                        for test_key in ['skip-if', 'fail-if']:
                            value = t.get(test_key)
                            if value:
                                test_info[test_key] = value
                        if t.get('fail-if'):
                            failed_count += 1
                        if t.get('skip-if'):
                            skipped_count += 1
                        if show_tests:
                            rkey = key if show_components else 'all'
                            if rkey in by_component['tests']:
                                by_component['tests'][rkey].append(test_info)
                            else:
                                by_component['tests'][rkey] = [test_info]
                        break
            if show_tests:
                for key in by_component['tests']:
                    by_component['tests'][key].sort()

        if show_summary:
            by_component['summary'] = {}
            by_component['summary']['components'] = len(component_set)
            by_component['summary']['manifests'] = manifest_count
            by_component['summary']['tests'] = test_count
            by_component['summary']['failed tests'] = failed_count
            by_component['summary']['skipped tests'] = skipped_count

        json_report = json.dumps(by_component, indent=2, sort_keys=True)
        if output_file:
            with open(output_file, 'w') as f:
                f.write(json_report)
        else:
            print(json_report)


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

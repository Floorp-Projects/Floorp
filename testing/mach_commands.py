# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import json
import logging
import os
import sys
import tempfile
import subprocess
import shutil

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SettingsProvider,
)

from mozbuild.base import MachCommandBase, MachCommandConditions as conditions
from moztest.resolve import TEST_SUITES
from argparse import ArgumentParser

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
I know you are trying to run a %s test. Unfortunately, I can't run those
tests yet. Sorry!
'''.strip()

TEST_HELP = '''
Test or tests to run. Tests can be specified by filename, directory, suite
name or suite alias.

The following test suites and aliases are supported: %s
''' % ', '.join(sorted(TEST_SUITES))
TEST_HELP = TEST_HELP.strip()


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
            ('test.format', 'string', format_desc, 'tbpl', {'choices': format_choices}),
            ('test.level', 'string', level_desc, 'info', {'choices': level_choices}),
        ]


@CommandProvider
class Test(MachCommandBase):
    @Command('test', category='testing',
             description='Run tests (detects the kind of test and runs it).')
    @CommandArgument('what', default=None, nargs='*', help=TEST_HELP)
    @CommandArgument('extra_args', default=None, nargs=argparse.REMAINDER,
                     help="Extra arguments to pass to the underlying test command(s). "
                          "If an underlying command doesn't recognize the argument, it "
                          "will fail.")
    def test(self, what, extra_args):
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
        from mozlog.commandline import log_formatters
        from mozlog.handlers import StreamHandler, LogLevelFilter
        from mozlog.structuredlog import StructuredLogger
        from moztest.resolve import TestResolver, TEST_FLAVORS, TEST_SUITES

        resolver = self._spawn(TestResolver)
        run_suites, run_tests = resolver.resolve_metadata(what)

        if not run_suites and not run_tests:
            print(UNKNOWN_TEST)
            return 1

        # Create shared logger
        formatter = log_formatters[self._mach_context.settings['test']['format']][0]()

        level = self._mach_context.settings['test']['level']
        log = StructuredLogger('mach-test')
        log.add_handler(StreamHandler(sys.stdout, LogLevelFilter(formatter, level)))

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
            if flavor not in TEST_FLAVORS:
                print(UNKNOWN_FLAVOR % flavor)
                status = 1
                continue

            m = TEST_FLAVORS[flavor]
            if 'mach_command' not in m:
                print(UNKNOWN_FLAVOR % flavor)
                status = 1
                continue

            kwargs = dict(m['kwargs'])
            kwargs['log'] = log
            kwargs['subsuite'] = subsuite

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

        if conditions.is_android(self):
            from mozrunner.devices.android_device import verify_android_device
            verify_android_device(self, install=False)
            return self.run_android_test(tests, symbols_path, manifest_path, log)

        return self.run_desktop_test(tests, symbols_path, manifest_path, log)

    def run_desktop_test(self, tests, symbols_path, manifest_path, log):
        import runcppunittests as cppunittests
        from mozlog import commandline

        parser = cppunittests.CPPUnittestOptions()
        commandline.add_logging_group(parser)
        options, args = parser.parse_args()

        options.symbols_path = symbols_path
        options.manifest_path = manifest_path
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
        jstest_cmd = [
            python,
            os.path.join(self.topsrcdir, 'js', 'src', 'tests', 'jstests.py'),
            js,
            '--jitflags=all',
        ]
        jstest_result = subprocess.call(jstest_cmd)

        print('running jsapi-tests')
        jsapi_tests_cmd = [os.path.join(
            self.bindir, executable_name('jsapi-tests'))]
        jsapi_tests_result = subprocess.call(jsapi_tests_cmd)

        print('running check-style')
        check_style_cmd = [python, os.path.join(
            self.topsrcdir, 'config', 'check_spidermonkey_style.py')]
        check_style_result = subprocess.call(
            check_style_cmd, cwd=os.path.join(self.topsrcdir, 'js', 'src'))

        print('running check-masm')
        check_masm_cmd = [python, os.path.join(
            self.topsrcdir, 'config', 'check_macroassembler_style.py')]
        check_masm_result = subprocess.call(
            check_masm_cmd, cwd=os.path.join(self.topsrcdir, 'js', 'src'))

        print('running check-js-msg-encoding')
        check_js_msg_cmd = [python, os.path.join(
            self.topsrcdir, 'config', 'check_js_msg_encoding.py')]
        check_js_msg_result = subprocess.call(
            check_js_msg_cmd, cwd=self.topsrcdir)

        all_passed = jittest_result and jstest_result and jsapi_tests_result and \
            check_style_result and check_masm_result and check_js_msg_result

        return all_passed


@CommandProvider
class JsapiTestsCommand(MachCommandBase):
    @Command('jsapi-tests', category='testing', description='Run jsapi tests (JavaScript engine).')
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


def get_parser(argv=None):
    parser = ArgumentParser()
    parser.add_argument(dest="suite_name",
                        nargs=1,
                        choices=['mochitest'],
                        type=str,
                        help="The test for which chunk should be found. It corresponds "
                             "to the mach test invoked (only 'mochitest' currently).")

    parser.add_argument(dest="test_path",
                        nargs=1,
                        type=str,
                        help="The test (any mochitest) for which chunk should be found.")

    parser.add_argument('--total-chunks',
                        type=int,
                        dest='total_chunks',
                        required=True,
                        help='Total number of chunks to split tests into.',
                        default=None)

    parser.add_argument('--chunk-by-runtime',
                        action='store_true',
                        dest='chunk_by_runtime',
                        help='Group tests such that each chunk has roughly the same runtime.',
                        default=False)

    parser.add_argument('--chunk-by-dir',
                        type=int,
                        dest='chunk_by_dir',
                        help='Group tests together in the same chunk that are in the same top '
                             'chunkByDir directories.',
                        default=None)

    parser.add_argument('--disable-e10s',
                        action='store_false',
                        dest='e10s',
                        help='Find test on chunk with electrolysis preferences disabled.',
                        default=True)

    parser.add_argument('-p', '--platform',
                        choices=['linux', 'linux64', 'mac',
                                 'macosx64', 'win32', 'win64'],
                        dest='platform',
                        help="Platform for the chunk to find the test.",
                        default=None)

    parser.add_argument('--debug',
                        action='store_true',
                        dest='debug',
                        help="Find the test on chunk in a debug build.",
                        default=False)

    return parser


def download_mozinfo(platform=None, debug_build=False):
    temp_dir = tempfile.mkdtemp()
    temp_path = os.path.join(temp_dir, "mozinfo.json")
    args = [
        'mozdownload',
        '-t', 'tinderbox',
        '--ext', 'mozinfo.json',
        '-d', temp_path,
    ]
    if platform:
        if platform == 'macosx64':
            platform = 'mac64'
        args.extend(['-p', platform])
    if debug_build:
        args.extend(['--debug-build'])

    subprocess.call(args)
    return temp_dir, temp_path


@CommandProvider
class ChunkFinder(MachCommandBase):
    @Command('find-test-chunk', category='testing',
             description='Find which chunk a test belongs to (works for mochitest).',
             parser=get_parser)
    def chunk_finder(self, **kwargs):
        total_chunks = kwargs['total_chunks']
        test_path = kwargs['test_path'][0]
        suite_name = kwargs['suite_name'][0]
        _, dump_tests = tempfile.mkstemp()

        from moztest.resolve import TestResolver
        resolver = self._spawn(TestResolver)
        relpath = self._wrap_path_argument(test_path).relpath()
        tests = list(resolver.resolve_tests(paths=[relpath]))
        if len(tests) != 1:
            print('No test found for test_path: %s' % test_path)
            sys.exit(1)

        flavor = tests[0]['flavor']
        subsuite = tests[0]['subsuite']
        args = {
            'totalChunks': total_chunks,
            'dump_tests': dump_tests,
            'chunkByDir': kwargs['chunk_by_dir'],
            'chunkByRuntime': kwargs['chunk_by_runtime'],
            'e10s': kwargs['e10s'],
            'subsuite': subsuite,
        }

        temp_dir = None
        if kwargs['platform'] or kwargs['debug']:
            self._activate_virtualenv()
            self.virtualenv_manager.install_pip_package('mozdownload==1.17')
            temp_dir, temp_path = download_mozinfo(
                kwargs['platform'], kwargs['debug'])
            args['extra_mozinfo_json'] = temp_path

        found = False
        for this_chunk in range(1, total_chunks + 1):
            args['thisChunk'] = this_chunk
            try:
                self._mach_context.commands.dispatch(
                    suite_name, self._mach_context, flavor=flavor, resolve_tests=False, **args)
            except SystemExit:
                pass
            except KeyboardInterrupt:
                break

            fp = open(os.path.expanduser(args['dump_tests']), 'r')
            tests = json.loads(fp.read())['active_tests']
            for test in tests:
                if test_path == test['path']:
                    if 'disabled' in test:
                        print('The test %s for flavor %s is disabled on the given platform' % (
                            test_path, flavor))
                    else:
                        print('The test %s for flavor %s is present in chunk number: %d' % (
                            test_path, flavor, this_chunk))
                    found = True
                    break

            if found:
                break

        if not found:
            raise Exception("Test %s not found." % test_path)
        # Clean up the file
        os.remove(dump_tests)
        if temp_dir:
            shutil.rmtree(temp_dir)


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
    @CommandArgument('--show-bugs', action='store_true',
                     help='Retrieve and display related Bugzilla bugs.')
    @CommandArgument('--verbose', action='store_true',
                     help='Enable debug logging.')
    def test_info(self, **params):

        import which
        from mozbuild.base import MozbuildObject

        self.branches = params['branches']
        self.start = params['start']
        self.end = params['end']
        self.show_info = params['show_info']
        self.show_results = params['show_results']
        self.show_durations = params['show_durations']
        self.show_bugs = params['show_bugs']
        self.verbose = params['verbose']

        if (not self.show_info and
            not self.show_results and
            not self.show_durations and
                not self.show_bugs):
            # by default, show everything
            self.show_info = True
            self.show_results = True
            self.show_durations = True
            self.show_bugs = True

        here = os.path.abspath(os.path.dirname(__file__))
        build_obj = MozbuildObject.from_environment(cwd=here)

        self._hg = None
        if conditions.is_hg(build_obj):
            if self._is_windows():
                self._hg = which.which('hg.exe')
            else:
                self._hg = which.which('hg')

        self._git = None
        if conditions.is_git(build_obj):
            if self._is_windows():
                self._git = which.which('git.exe')
            else:
                self._git = which.which('git')

        for test_name in params['test_names']:
            print("===== %s =====" % test_name)
            self.test_name = test_name
            if len(self.test_name) < 6:
                print("'%s' is too short for a test name!" % self.test_name)
                continue
            if self.show_info:
                self.set_test_name()
            if self.show_results:
                self.report_test_results()
            if self.show_durations:
                self.report_test_durations()
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

        if not (self.show_results or self.show_durations):
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
        platform = record['build']['platform']
        type = record['build']['type']
        if 'run' in record and 'e10s' in record['run']['type']:
            e10s = "-e10s"
        else:
            e10s = ""
        return "%s/%s%s:" % (platform, type, e10s)

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
                runs = record['count']
                total_runs = total_runs + runs
                failures = record['failures']
                total_failures = total_failures + failures
                rate = (float)(failures) / runs
                if rate >= worst_rate:
                    worst_rate = rate
                    worst_platform = platform
                    worst_failures = failures
                    worst_runs = runs
                print("%-40s %6d failures in %6d runs" % (
                    platform, failures, runs))
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
                print("%-40s %6.2f s (%.2f s - %.2f s over %d runs)" % (
                    platform, record['average'], record['min'],
                    record['max'], record['count']))
        else:
            print("No test durations found.")

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

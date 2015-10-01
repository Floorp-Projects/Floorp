# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import os
import sys
import tempfile
import subprocess
import shutil
from collections import defaultdict

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase
from mozbuild.base import MachCommandConditions as conditions
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

MOCHITEST_CHUNK_BY_DIR = 4
MOCHITEST_TOTAL_CHUNKS = 5

TEST_SUITES = {
    'cppunittest': {
        'aliases': ('Cpp', 'cpp'),
        'mach_command': 'cppunittest',
        'kwargs': {'test_file': None},
    },
    'crashtest': {
        'aliases': ('C', 'Rc', 'RC', 'rc'),
        'mach_command': 'crashtest',
        'kwargs': {'test_file': None},
    },
    'crashtest-ipc': {
        'aliases': ('Cipc', 'cipc'),
        'mach_command': 'crashtest-ipc',
        'kwargs': {'test_file': None},
    },
    'jetpack': {
        'aliases': ('J',),
        'mach_command': 'jetpack-test',
        'kwargs': {},
    },
    'check-spidermonkey': {
        'aliases': ('Sm', 'sm'),
        'mach_command': 'check-spidermonkey',
        'kwargs': {'valgrind': False},
    },
    'mochitest-a11y': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'a11y', 'test_paths': None},
    },
    'mochitest-browser': {
        'aliases': ('bc', 'BC', 'Bc'),
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'browser-chrome', 'test_paths': None},
    },
    'mochitest-chrome': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'chrome', 'test_paths': None},
    },
    'mochitest-devtools': {
        'aliases': ('dt', 'DT', 'Dt'),
        'mach_command': 'mochitest',
        'kwargs': {'subsuite': 'devtools', 'test_paths': None},
    },
    'mochitest-plain': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'plain', 'test_paths': None},
    },
    'luciddream': {
        'mach_command': 'luciddream',
        'kwargs': {'test_paths': None},
    },
    'reftest': {
        'aliases': ('RR', 'rr', 'Rr'),
        'mach_command': 'reftest',
        'kwargs': {'tests': None},
    },
    'reftest-ipc': {
        'aliases': ('Ripc',),
        'mach_command': 'reftest-ipc',
        'kwargs': {'test_file': None},
    },
    'web-platform-tests': {
        'aliases': ('wpt',),
        'mach_command': 'web-platform-tests',
        'kwargs': {}
    },
    'valgrind': {
        'aliases': ('V', 'v'),
        'mach_command': 'valgrind-test',
        'kwargs': {},
    },
    'xpcshell': {
        'aliases': ('X', 'x'),
        'mach_command': 'xpcshell-test',
        'kwargs': {'test_file': 'all'},
    },
}

# Maps test flavors to metadata on how to run that test.
TEST_FLAVORS = {
    'a11y': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'a11y', 'test_paths': []},
    },
    'browser-chrome': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'browser-chrome', 'test_paths': []},
    },
    'chrashtest': { },
    'chrome': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'chrome', 'test_paths': []},
    },
    'mochitest': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'mochitest', 'test_paths': []},
    },
    'reftest': {
        'mach_command': 'reftest',
        'kwargs': {'tests': []}
    },
    'steeplechase': { },
    'web-platform-tests': {
        'mach_command': 'web-platform-tests',
        'kwargs': {'include': []}
    },
    'webapprt-chrome': {
        'mach_command': 'mochitest',
        'kwargs': {'flavor': 'webapprt-chrome', 'test_paths': []},
    },
    'xpcshell': {
        'mach_command': 'xpcshell-test',
        'kwargs': {'test_paths': []},
    },
}


for i in range(1, MOCHITEST_TOTAL_CHUNKS + 1):
    TEST_SUITES['mochitest-%d' %i] = {
        'aliases': ('M%d' % i, 'm%d' % i),
        'mach_command': 'mochitest',
        'kwargs': {
            'flavor': 'mochitest',
            'subsuite': 'default',
            'chunk_by_dir': MOCHITEST_CHUNK_BY_DIR,
            'total_chunks': MOCHITEST_TOTAL_CHUNKS,
            'this_chunk': i,
            'test_paths': None,
        },
    }

TEST_HELP = '''
Test or tests to run. Tests can be specified by filename, directory, suite
name or suite alias.

The following test suites and aliases are supported: %s
''' % ', '.join(sorted(TEST_SUITES))
TEST_HELP = TEST_HELP.strip()


@CommandProvider
class Test(MachCommandBase):
    @Command('test', category='testing', description='Run tests (detects the kind of test and runs it).')
    @CommandArgument('what', default=None, nargs='*', help=TEST_HELP)
    def test(self, what):
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
        from mozbuild.testing import TestResolver

        # Parse arguments and assemble a test "plan."
        run_suites = set()
        run_tests = []
        resolver = self._spawn(TestResolver)

        for entry in what:
            # If the path matches the name or alias of an entire suite, run
            # the entire suite.
            if entry in TEST_SUITES:
                run_suites.add(entry)
                continue
            suitefound = False
            for suite, v in TEST_SUITES.items():
                if entry in v.get('aliases', []):
                    run_suites.add(suite)
                    suitefound = True
            if suitefound:
                continue

            # Now look for file/directory matches in the TestResolver.
            relpath = self._wrap_path_argument(entry).relpath()
            tests = list(resolver.resolve_tests(paths=[relpath]))
            run_tests.extend(tests)

            if not tests:
                print('UNKNOWN TEST: %s' % entry, file=sys.stderr)

        if not what:
            # TODO: This isn't really related to try, and should be
            # extracted to a common library for vcs interactions when it is
            # introduced in bug 1185599.
            from autotry import AutoTry
            at = AutoTry(self.topsrcdir, resolver, self._mach_context)
            changed_files = at.find_changed_files()
            if changed_files:
                print("Tests will be run based on modifications to the "
                      "following files:\n\t%s" % "\n\t".join(changed_files))

            from mozbuild.frontend.reader import (
                BuildReader,
                EmptyConfig,
            )
            config = EmptyConfig(self.topsrcdir)
            reader = BuildReader(config)
            files_info = reader.files_info(changed_files)

            paths, tags, flavors = set(), set(), set()
            for info in files_info.values():
                paths |= info.test_files
                tags |= info.test_tags
                flavors |= info.test_flavors

            # This requires multiple calls to resolve_tests, because the test
            # resolver returns tests that match every condition, while we want
            # tests that match any condition. Bug 1210213 tracks implementing
            # more flexible querying.
            if tags:
                run_tests = list(resolver.resolve_tests(tags=tags))
            if paths:
                run_tests += [t for t in resolver.resolve_tests(paths=paths)
                              if not (tags & set(t.get('tags', '').split()))]
            if flavors:
                run_tests = [t for t in run_tests if t['flavor'] not in flavors]
                for flavor in flavors:
                    run_tests += list(resolver.resolve_tests(flavor=flavor))

        if not run_suites and not run_tests:
            print(UNKNOWN_TEST)
            return 1

        status = None
        for suite_name in run_suites:
            suite = TEST_SUITES[suite_name]

            if 'mach_command' in suite:
                res = self._mach_context.commands.dispatch(
                    suite['mach_command'], self._mach_context,
                    **suite['kwargs'])
                if res:
                    status = res

        buckets = {}
        for test in run_tests:
            key = (test['flavor'], test['subsuite'])
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
            kwargs['subsuite'] = subsuite

            res = self._mach_context.commands.dispatch(
                    m['mach_command'], self._mach_context,
                    test_objects=tests, **kwargs)
            if res:
                status = res

        return status


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('cppunittest', category='testing',
        description='Run cpp unit tests (C++ tests).')
    @CommandArgument('test_files', nargs='*', metavar='N',
        help='Test to run. Can be specified as one or more files or ' \
            'directories, or omitted. If omitted, the entire test suite is ' \
            'executed.')

    def run_cppunit_test(self, **params):
        import mozinfo
        from mozlog import commandline
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
            manifest_path = os.path.join(self.topsrcdir, 'testing', 'cppunittest.ini')
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

        options.symbols_path = symbols_path
        options.manifest_path = manifest_path
        options.xre_path = self.bindir
        options.dm_trans = "adb"
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
    @Command('check-spidermonkey', category='testing', description='Run SpiderMonkey tests (JavaScript engine).')
    @CommandArgument('--valgrind', action='store_true', help='Run jit-test suite with valgrind flag')

    def run_checkspidermonkey(self, **params):
        import subprocess
        import sys

        js = os.path.join(self.bindir, executable_name('js'))

        print('Running jit-tests')
        jittest_cmd = [os.path.join(self.topsrcdir, 'js', 'src', 'jit-test', 'jit_test.py'),
              js, '--no-slow', '--jitflags=all']
        if params['valgrind']:
            jittest_cmd.append('--valgrind')

        jittest_result = subprocess.call(jittest_cmd)

        print('running jstests')
        jstest_cmd = [os.path.join(self.topsrcdir, 'js', 'src', 'tests', 'jstests.py'),
              js, '--jitflags=all']
        jstest_result = subprocess.call(jstest_cmd)

        print('running jsapi-tests')
        jsapi_tests_cmd = [os.path.join(self.bindir, executable_name('jsapi-tests'))]
        jsapi_tests_result = subprocess.call(jsapi_tests_cmd)

        print('running check-style')
        check_style_cmd = [sys.executable, os.path.join(self.topsrcdir, 'config', 'check_spidermonkey_style.py')]
        check_style_result = subprocess.call(check_style_cmd, cwd=os.path.join(self.topsrcdir, 'js', 'src'))

        print('running check-masm')
        check_masm_cmd = [sys.executable, os.path.join(self.topsrcdir, 'config', 'check_macroassembler_style.py')]
        check_masm_result = subprocess.call(check_masm_cmd, cwd=os.path.join(self.topsrcdir, 'js', 'src'))

        all_passed = jittest_result and jstest_result and jsapi_tests_result and check_style_result and check_masm_result

        return all_passed

@CommandProvider
class JsapiTestsCommand(MachCommandBase):
    @Command('jsapi-tests', category='testing', description='Run jsapi tests (JavaScript engine).')
    @CommandArgument('test_name', nargs='?', metavar='N',
        help='Test to run. Can be a prefix or omitted. If omitted, the entire ' \
             'test suite is executed.')

    def run_jsapitests(self, **params):
        import subprocess

        bin_suffix = ''
        if sys.platform.startswith('win'):
            bin_suffix = '.exe'

        print('running jsapi-tests')
        jsapi_tests_cmd = [os.path.join(self.bindir, executable_name('jsapi-tests'))]
        if params['test_name']:
            jsapi_tests_cmd.append(params['test_name'])

        jsapi_tests_result = subprocess.call(jsapi_tests_cmd)

        return jsapi_tests_result

def autotry_parser():
    from autotry import arg_parser
    return arg_parser()

@CommandProvider
class PushToTry(MachCommandBase):
    def normalise_list(self, items, allow_subitems=False):
        from autotry import parse_arg

        rv = defaultdict(list)
        for item in items:
            parsed = parse_arg(item)
            for key, values in parsed.iteritems():
                rv[key].extend(values)

        if not allow_subitems:
            if not all(item == [] for item in rv.itervalues()):
                raise ValueError("Unexpected subitems in argument")
            return rv.keys()
        else:
            return rv

    def validate_args(self, **kwargs):
        if not kwargs["paths"] and not kwargs["tests"] and not kwargs["tags"]:
            print("Paths, tags, or tests must be specified as an argument to autotry.")
            sys.exit(1)

        if kwargs["platforms"] is None:
            if 'AUTOTRY_PLATFORM_HINT' in os.environ:
                kwargs["platforms"] = [os.environ['AUTOTRY_PLATFORM_HINT']]
            else:
                print("Platforms must be specified as an argument to autotry.")
                sys.exit(1)

        try:
            platforms = self.normalise_list(kwargs["platforms"])
        except ValueError as e:
            print("Error parsing -p argument:\n%s" % e.message)
            sys.exit(1)

        try:
            tests = (self.normalise_list(kwargs["tests"], allow_subitems=True)
                     if kwargs["tests"] else {})
        except ValueError as e:
            print("Error parsing -u argument (%s):\n%s" % (kwargs["tests"], e.message))
            sys.exit(1)

        try:
            talos = self.normalise_list(kwargs["talos"]) if kwargs["talos"] else []
        except ValueError as e:
            print("Error parsing -t argument:\n%s" % e.message)
            sys.exit(1)

        paths = []
        for p in kwargs["paths"]:
            p = os.path.normpath(os.path.abspath(p))
            if not p.startswith(self.topsrcdir):
                print('Specified path "%s" is outside of the srcdir, unable to'
                      ' specify tests outside of the srcdir' % p)
                sys.exit(1)
            if len(p) <= len(self.topsrcdir):
                print('Specified path "%s" is at the top of the srcdir and would'
                      ' select all tests.' % p)
                sys.exit(1)
            paths.append(os.path.relpath(p, self.topsrcdir))

        try:
            tags = self.normalise_list(kwargs["tags"]) if kwargs["tags"] else []
        except ValueError as e:
            print("Error parsing --tags argument:\n%s" % e.message)
            sys.exit(1)

        return kwargs["builds"], platforms, tests, talos, paths, tags, kwargs["extra_args"]


    @Command('try',
             category='testing',
             description='Push selected tests to the try server',
             parser=autotry_parser)

    def autotry(self, **kwargs):
        """Autotry is in beta, please file bugs blocking 1149670.

        Push the current tree to try, with the specified syntax.

        Build options, platforms and regression tests may be selected
        using the usual try options (-b, -p and -u respectively). In
        addition, tests in a given directory may be automatically
        selected by passing that directory as a positional argument to the
        command. For example:

        mach try -b d -p linux64 dom testing/web-platform/tests/dom

        would schedule a try run for linux64 debug consisting of all
        tests under dom/ and testing/web-platform/tests/dom.

        Test selection using positional arguments is available for
        mochitests, reftests, xpcshell tests and web-platform-tests.

        Tests may be also filtered by passing --tag to the command,
        which will run only tests marked as having the specified
        tags e.g.

        mach try -b d -p win64 --tag media

        would run all tests tagged 'media' on Windows 64.

        If both positional arguments or tags and -u are supplied, the
        suites in -u will be run in full. Where tests are selected by
        positional argument they will be run in a single chunk.

        If no build option is selected, both debug and opt will be
        scheduled. If no platform is selected a default is taken from
        the AUTOTRY_PLATFORM_HINT environment variable, if set.

        The command requires either its own mercurial extension ("push-to-try",
        installable from mach mercurial-setup) or a git repo using git-cinnabar
        (available at https://github.com/glandium/git-cinnabar).

        """

        from mozbuild.testing import TestResolver
        from mozbuild.controller.building import BuildDriver
        from autotry import AutoTry

        print("mach try is under development, please file bugs blocking 1149670.")

        resolver = self._spawn(TestResolver)
        at = AutoTry(self.topsrcdir, resolver, self._mach_context)

        if kwargs["load"] is not None:
            defaults = at.load_config(kwargs["load"])

            if defaults is None:
                print("No saved configuration called %s found in autotry.ini" % kwargs["load"],
                      file=sys.stderr)

            for key, value in kwargs.iteritems():
                if value in (None, []) and key in defaults:
                    kwargs[key] = defaults[key]

        if kwargs["push"] and at.find_uncommited_changes():
            print('ERROR please commit changes before continuing')
            sys.exit(1)

        if not any(kwargs[item] for item in ("paths", "tests", "tags")):
            kwargs["paths"], kwargs["tags"] = at.find_paths_and_tags(kwargs["verbose"])

        builds, platforms, tests, talos, paths, tags, extra_args = self.validate_args(**kwargs)

        if paths or tags:
            driver = self._spawn(BuildDriver)
            driver.install_tests(remove=False)

            paths = [os.path.relpath(os.path.normpath(os.path.abspath(item)), self.topsrcdir)
                     for item in paths]
            paths_by_flavor = at.paths_by_flavor(paths=paths, tags=tags)

            if not paths_by_flavor and not tests:
                print("No tests were found when attempting to resolve paths:\n\n\t%s" %
                      paths)
                sys.exit(1)

            if not kwargs["intersection"]:
                paths_by_flavor = at.remove_duplicates(paths_by_flavor, tests)
        else:
            paths_by_flavor = {}

        try:
            msg = at.calc_try_syntax(platforms, tests, talos, builds, paths_by_flavor, tags,
                                     extra_args, kwargs["intersection"])
        except ValueError as e:
            print(e.message)
            sys.exit(1)

        if kwargs["verbose"] and paths_by_flavor:
            print('The following tests will be selected: ')
            for flavor, paths in paths_by_flavor.iteritems():
                print("%s: %s" % (flavor, ",".join(paths)))

        if kwargs["verbose"] or not kwargs["push"]:
            print('The following try syntax was calculated:\n%s' % msg)

        if kwargs["push"]:
            at.push_to_try(msg, kwargs["verbose"])

        if kwargs["save"] is not None:
            at.save_config(kwargs["save"], msg)


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

    parser.add_argument('--e10s',
                        action='store_true',
                        dest='e10s',
                        help='Find test on chunk with electrolysis preferences enabled.',
                        default=False)

    parser.add_argument('-p', '--platform',
                        choices=['linux', 'linux64', 'mac', 'macosx64', 'win32', 'win64'],
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

        from mozbuild.testing import TestResolver
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
            temp_dir, temp_path = download_mozinfo(kwargs['platform'], kwargs['debug'])
            args['extra_mozinfo_json'] = temp_path

        found = False
        for this_chunk in range(1, total_chunks+1):
            args['thisChunk'] = this_chunk
            try:
                self._mach_context.commands.dispatch(suite_name, self._mach_context, flavor=flavor, resolve_tests=False, **args)
            except SystemExit:
                pass
            except KeyboardInterrupt:
                break

            fp = open(os.path.expanduser(args['dump_tests']), 'r')
            tests = json.loads(fp.read())['active_tests']
            for test in tests:
                if test_path == test['path']:
                    if 'disabled' in test:
                        print('The test %s for flavor %s is disabled on the given platform' % (test_path, flavor))
                    else:
                        print('The test %s for flavor %s is present in chunk number: %d' % (test_path, flavor, this_chunk))
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

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from argparse import ArgumentParser

import json
import mozinfo
import moznetwork
import os
import random
import socket
import sys
import time
import traceback
import unittest
import warnings
import mozprofile


from manifestparser import TestManifest
from manifestparser.filters import tags
from marionette_driver.marionette import Marionette
from mozlog import get_default_logger
from moztest.adapters.unit import StructuredTestRunner, StructuredTestResult
from moztest.results import TestResultCollection, TestResult, relevant_line
import mozversion

from marionette.runner import httpd


here = os.path.abspath(os.path.dirname(__file__))


class SessionTest(TestResult):

    @property
    def test_name(self):
        if self.test_class is not None:
            return '%s.py %s.%s' % (self.test_class.split('.')[0],
                                    self.test_class,
                                    self.name)
        else:
            return self.name

class SessionTestResult(StructuredTestResult, TestResultCollection):

    resultClass = SessionTest

    def __init__(self, *args, **kwargs):
        TestResultCollection.__init__(self, 'SessionTest')
        self.passed = 0
        self.testsRun = 0
        self.result_modifiers = [] # used by mixins to modify the result
        StructuredTestResult.__init__(self, *args, **kwargs)

    @property
    def skipped(self):
        return [t for t in self if t.result == 'SKIPPED']

    @skipped.setter
    def skipped(self, value):
        pass

    @property
    def expectedFailures(self):
        return [t for t in self if t.result == 'KNOWN-FAIL']

    @expectedFailures.setter
    def expectedFailures(self, value):
        pass

    @property
    def unexpectedSuccesses(self):
        return [t for t in self if t.result == 'UNEXPECTED-PASS']

    @unexpectedSuccesses.setter
    def unexpectedSuccesses(self, value):
        pass

    @property
    def tests_passed(self):
        return [t for t in self if t.result == 'PASS']

    @property
    def errors(self):
        return [t for t in self if t.result == 'ERROR']

    @errors.setter
    def errors(self, value):
        pass

    @property
    def failures(self):
        return [t for t in self if t.result == 'UNEXPECTED-FAIL']

    @failures.setter
    def failures(self, value):
        pass

    @property
    def duration(self):
        if self.stop_time:
            return self.stop_time - self.start_time
        else:
            return 0

    def add_test_result(self, test, result_expected='PASS',
                        result_actual='PASS', output='', context=None, **kwargs):
        def get_class(test):
            return test.__class__.__module__ + '.' + test.__class__.__name__

        name = str(test).split()[0]
        test_class = get_class(test)
        if hasattr(test, 'jsFile'):
            name = os.path.basename(test.jsFile)
            test_class = None

        t = self.resultClass(name=name, test_class=test_class,
                       time_start=test.start_time, result_expected=result_expected,
                       context=context, **kwargs)
        # call any registered result modifiers
        for modifier in self.result_modifiers:
            result_expected, result_actual, output, context = modifier(t, result_expected, result_actual, output, context)
        t.finish(result_actual,
                 time_end=time.time() if test.start_time else 0,
                 reason=relevant_line(output),
                 output=output)
        self.append(t)

    def addError(self, test, err):
        self.add_test_result(test, output=self._exc_info_to_string(err, test), result_actual='ERROR')
        super(SessionTestResult, self).addError(test, err)

    def addFailure(self, test, err):
        self.add_test_result(test, output=self._exc_info_to_string(err, test), result_actual='UNEXPECTED-FAIL')
        super(SessionTestResult, self).addFailure(test, err)

    def addSuccess(self, test):
        self.passed += 1
        self.add_test_result(test, result_actual='PASS')
        super(SessionTestResult, self).addSuccess(test)

    def addExpectedFailure(self, test, err):
        """Called when an expected failure/error occured."""
        self.add_test_result(test, output=self._exc_info_to_string(err, test),
                             result_actual='KNOWN-FAIL')
        super(SessionTestResult, self).addExpectedFailure(test, err)

    def addUnexpectedSuccess(self, test):
        """Called when a test was expected to fail, but succeed."""
        self.add_test_result(test, result_actual='UNEXPECTED-PASS')
        super(SessionTestResult, self).addUnexpectedSuccess(test)

    def addSkip(self, test, reason):
        self.add_test_result(test, output=reason, result_actual='SKIPPED')
        super(SessionTestResult, self).addSkip(test, reason)

    def getInfo(self, test):
        return test.test_name

    def getDescription(self, test):
        doc_first_line = test.shortDescription()
        if self.descriptions and doc_first_line:
            return '\n'.join((str(test), doc_first_line))
        else:
            desc = str(test)
            if hasattr(test, 'jsFile'):
                desc = "%s, %s" % (test.jsFile, desc)
            return desc

    def printLogs(self, test):
        for testcase in test._tests:
            if hasattr(testcase, 'loglines') and testcase.loglines:
                # Don't dump loglines to the console if they only contain
                # TEST-START and TEST-END.
                skip_log = True
                for line in testcase.loglines:
                    str_line = ' '.join(line)
                    if not 'TEST-END' in str_line and not 'TEST-START' in str_line:
                        skip_log = False
                        break
                if skip_log:
                    return
                self.logger.info('START LOG:')
                for line in testcase.loglines:
                    self.logger.info(' '.join(line).encode('ascii', 'replace'))
                self.logger.info('END LOG:')

    def stopTest(self, *args, **kwargs):
        unittest._TextTestResult.stopTest(self, *args, **kwargs)


class SessionTextTestRunner(StructuredTestRunner):

    resultclass = SessionTestResult

    def __init__(self, **kwargs):
        self.binary = kwargs.pop('binary')
        StructuredTestRunner.__init__(self, **kwargs)

    def _makeResult(self):
        return self.resultclass(self.stream,
                                self.descriptions,
                                self.verbosity,
                                logger=self.logger,
                                result_callbacks=self.result_callbacks)

    def run(self, test):
        result = super(SessionTextTestRunner, self).run(test)
        result.printLogs(test)
        return result


class BaseSessionArguments(ArgumentParser):
    socket_timeout_default = 360.0

    def __init__(self, **kwargs):
        ArgumentParser.__init__(self, **kwargs)

        def dir_path(path):
            path = os.path.abspath(os.path.expanduser(path))
            if not os.access(path, os.F_OK):
                os.makedirs(path)
            return path

        self.argument_containers = []
        self.add_argument('tests',
                          nargs='*',
                          default=[],
                          help='Tests to run.')
        self.add_argument('-v', '--verbose',
                        action='count',
                        help='Increase verbosity to include debug messages with -v, '
                            'and trace messages with -vv.')
        self.add_argument('--app',
                        help='application to use')
        self.add_argument('--app-arg',
                        dest='app_args',
                        action='append',
                        default=[],
                        help='specify a command line argument to be passed onto the application')
        self.add_argument('--binary',
                        help='gecko executable to launch before running the test')
        self.add_argument('--profile',
                        help='profile to use when launching the gecko process. if not passed, then a profile will be '
                             'constructed and used',
                        type=dir_path)
        self.add_argument('--pref',
                        action='append',
                        dest='prefs_args',
                        help=(" A preference to set. Must be a key-value pair"
                              " separated by a ':'."))
        self.add_argument('--preferences',
                        action='append',
                        dest='prefs_files',
                        help=("read preferences from a JSON or INI file. For"
                              " INI, use 'file.ini:section' to specify a"
                              " particular section."))
        self.add_argument('--addon',
                        action='append',
                        help="addon to install; repeat for multiple addons.")
        self.add_argument('--repeat',
                        type=int,
                        default=0,
                        help='number of times to repeat the test(s)')
        self.add_argument('--testvars',
                        action='append',
                        help='path to a json file with any test data required')
        self.add_argument('--symbols-path',
                        help='absolute path to directory containing breakpad symbols, or the url of a zip file containing symbols')
        self.add_argument('--timeout',
                        type=int,
                        help='if a --timeout value is given, it will set the default page load timeout, search timeout and script timeout to the given value. If not passed in, it will use the default values of 30000ms for page load, 0ms for search timeout and 10000ms for script timeout')
        self.add_argument('--startup-timeout',
                        type=int,
                        default=60,
                        help='the max number of seconds to wait for a Marionette connection after launching a binary')
        self.add_argument('--shuffle',
                        action='store_true',
                        default=False,
                        help='run tests in a random order')
        self.add_argument('--shuffle-seed',
                        type=int,
                        default=random.randint(0, sys.maxint),
                        help='Use given seed to shuffle tests')
        self.add_argument('--total-chunks',
                        type=int,
                        help='how many chunks to split the tests up into')
        self.add_argument('--this-chunk',
                        type=int,
                        help='which chunk to run')
        self.add_argument('--server-root',
                        help='url to a webserver or path to a document root from which content '
                        'resources are served (default: {}).'.format(os.path.join(
                            os.path.dirname(here), 'www')))
        self.add_argument('--gecko-log',
                        help="Define the path to store log file. If the path is"
                             " a directory, the real log file will be created"
                             " given the format gecko-(timestamp).log. If it is"
                             " a file, if will be used directly. '-' may be passed"
                             " to write to stdout. Default: './gecko.log'")
        self.add_argument('--logger-name',
                        default='Marionette-based Tests',
                        help='Define the name to associate with the logger used')
        self.add_argument('--jsdebugger',
                        action='store_true',
                        default=False,
                        help='Enable the jsdebugger for marionette javascript.')
        self.add_argument('--pydebugger',
                        help='Enable python post-mortem debugger when a test fails.'
                             ' Pass in the debugger you want to use, eg pdb or ipdb.')
        self.add_argument('--socket-timeout',
                        default=self.socket_timeout_default,
                        help='Set the global timeout for marionette socket operations.')
        self.add_argument('--disable-e10s',
                        action='store_false',
                        dest='e10s',
                        default=True,
                        help='Disable e10s when running marionette tests.')
        self.add_argument('--tag',
                        action='append', dest='test_tags',
                        default=None,
                        help="Filter out tests that don't have the given tag. Can be "
                             "used multiple times in which case the test must contain "
                             "at least one of the given tags.")
        self.add_argument('--workspace',
                          action='store',
                          default=None,
                          help="Path to directory for Marionette output. "
                               "(Default: .) (Default profile dest: TMP)",
                          type=dir_path)

    def register_argument_container(self, container):
        group = self.add_argument_group(container.name)

        for cli, kwargs in container.args:
            group.add_argument(*cli, **kwargs)

        self.argument_containers.append(container)

    def parse_args(self, args=None, values=None):
        args = ArgumentParser.parse_args(self, args, values)
        for container in self.argument_containers:
            if hasattr(container, 'parse_args_handler'):
                container.parse_args_handler(args)
        return args

    def _get_preferences(self, prefs_files, prefs_args):
        """
        return user defined profile preferences as a dict
        """
        # object that will hold the preferences
        prefs = mozprofile.prefs.Preferences()

        # add preferences files
        if prefs_files:
            for prefs_file in prefs_files:
                prefs.add_file(prefs_file)

        separator = ':'
        cli_prefs = []
        if prefs_args:
            for pref in prefs_args:
                if separator not in pref:
                    continue
                cli_prefs.append(pref.split(separator, 1))

        # string preferences
        prefs.add(cli_prefs, cast=True)

        return dict(prefs())

    def verify_usage(self, args):
        if not args.tests:
            print 'must specify one or more test files, manifests, or directories'
            sys.exit(1)

        for path in args.tests:
            if not os.path.exists(path):
                print '{0} does not exist'.format(path)
                sys.exit(1)

        if not args.binary:
            print 'must specify --binary'
            sys.exit(1)

        if args.total_chunks is not None and args.this_chunk is None:
            self.error('You must specify which chunk to run.')

        if args.this_chunk is not None and args.total_chunks is None:
            self.error('You must specify how many chunks to split the tests into.')

        if args.total_chunks is not None:
            if not 1 <= args.total_chunks:
                self.error('Total chunks must be greater than 1.')
            if not 1 <= args.this_chunk <= args.total_chunks:
                self.error('Chunk to run must be between 1 and %s.' % args.total_chunks)

        if args.jsdebugger:
            args.app_args.append('-jsdebugger')
            args.socket_timeout = None

        args.prefs = self._get_preferences(args.prefs_files, args.prefs_args)

        if args.e10s:
            args.prefs.update({
                'browser.tabs.remote.autostart': True,
                'browser.tabs.remote.force-enable': True,
                'extensions.e10sBlocksEnabling': False
            })

        for container in self.argument_containers:
            if hasattr(container, 'verify_usage_handler'):
                container.verify_usage_handler(args)

        return args


class BaseSessionTestRunner(object):

    textrunnerclass = SessionTextTestRunner
    driverclass = Marionette

    def __init__(self, address=None,
                 app=None, app_args=None, binary=None, profile=None,
                 logger=None, logdir=None,
                 repeat=0, testvars=None,
                 symbols_path=None, timeout=None,
                 shuffle=False, shuffle_seed=random.randint(0, sys.maxint),
                 sdcard=None, this_chunk=1, total_chunks=1, sources=None,
                 server_root=None, gecko_log=None, result_callbacks=None,
                 prefs=None, test_tags=None,
                 socket_timeout=BaseSessionArguments.socket_timeout_default,
                 startup_timeout=None, addons=None, workspace=None,
                 verbose=0, e10s=True, **kwargs):
        self.address = address
        self.app = app
        self.app_args = app_args or []
        self.bin = binary
        self.profile = profile
        self.addons = addons
        self.logger = logger
        self.httpd = None
        self.logdir = logdir
        self.repeat = repeat
        self.test_kwargs = kwargs
        self.symbols_path = symbols_path
        self.timeout = timeout
        self.socket_timeout = socket_timeout
        self._appinfo = None
        self._appName = None
        self.shuffle = shuffle
        self.shuffle_seed = shuffle_seed
        self.sdcard = sdcard
        self.sources = sources
        self.server_root = server_root
        self.this_chunk = this_chunk
        self.total_chunks = total_chunks
        self.mixin_run_tests = []
        self.manifest_skipped_tests = []
        self.tests = []
        self.result_callbacks = result_callbacks or []
        self.prefs = prefs or {}
        self.test_tags = test_tags
        self.startup_timeout = startup_timeout
        self.workspace = workspace
        # If no workspace is set, default location for gecko.log is .
        # and default location for profile is TMP
        self.workspace_path = workspace or os.getcwd()
        self.verbose = verbose
        self.e10s = e10s

        def gather_debug(test, status):
            rv = {}
            marionette = test.marionette

            # In the event we're gathering debug without starting a session, skip marionette commands
            if marionette.session is not None:
                try:
                    with marionette.using_context(marionette.CONTEXT_CHROME):
                        rv['screenshot'] = marionette.screenshot()
                    with marionette.using_context(marionette.CONTEXT_CONTENT):
                        rv['source'] = marionette.page_source
                except Exception:
                    logger = get_default_logger()
                    logger.warning('Failed to gather test failure debug.', exc_info=True)
            return rv

        self.result_callbacks.append(gather_debug)

        # testvars are set up in self.testvars property
        self._testvars = None
        self.testvars_paths = testvars

        self.test_handlers = []

        self.reset_test_stats()

        self.logger.info('Using workspace for temporary data: '
                         '"{}"'.format(self.workspace_path))

        if not gecko_log:
            self.gecko_log = os.path.join(self.workspace_path or '', 'gecko.log')
        else:
            self.gecko_log = gecko_log

        self.results = []

    @property
    def testvars(self):
        if self._testvars is not None:
            return self._testvars

        self._testvars = {}

        def update(d, u):
            """ Update a dictionary that may contain nested dictionaries. """
            for k, v in u.iteritems():
                o = d.get(k, {})
                if isinstance(v, dict) and isinstance(o, dict):
                    d[k] = update(d.get(k, {}), v)
                else:
                    d[k] = u[k]
            return d

        json_testvars = self._load_testvars()
        for j in json_testvars:
            self._testvars = update(self._testvars, j)
        return self._testvars

    def _load_testvars(self):
        data = []
        if self.testvars_paths is not None:
            for path in list(self.testvars_paths):
                path = os.path.abspath(os.path.expanduser(path))
                if not os.path.exists(path):
                    raise IOError('--testvars file %s does not exist' % path)
                try:
                    with open(path) as f:
                        data.append(json.loads(f.read()))
                except ValueError as e:
                    raise Exception("JSON file (%s) is not properly "
                                    "formatted: %s" % (os.path.abspath(path),
                                                       e.message))
        return data

    @property
    def bin(self):
        return self._bin

    @bin.setter
    def bin(self, path):
        """
        Set binary and reset parts of runner accordingly

        Intended use: to change binary between calls to run_tests
        """
        self._bin = path
        self.tests = []

    def reset_test_stats(self):
        self.passed = 0
        self.failed = 0
        self.crashed = 0
        self.unexpected_successes = 0
        self.todo = 0
        self.skipped = 0
        self.failures = []

    def run_tests(self, tests):
        assert len(tests) > 0
        assert len(self.test_handlers) > 0
        self.reset_test_stats()
        self.start_time = time.time()

        # Gaia sets server_root and that means we shouldn't spin up our own httpd
        if not self.httpd:
            if self.server_root is None or os.path.isdir(self.server_root):
                self.logger.info("starting httpd")
                self.httpd = self.create_httpd(False)
                self.base_url = self.httpd.get_url()
                self.logger.info("running httpd on %s" % self.base_url)
            else:
                self.base_url = self.server_root
                self.logger.info("using remote content from %s" % self.base_url)

        device_info = None

        for test in tests:
            self.add_test(test)

        # ensure we have only tests files with names starting with 'test_'
        invalid_tests = \
            [t['filepath'] for t in self.tests
             if not os.path.basename(t['filepath']).startswith('test_')]
        if invalid_tests:
            raise Exception("Tests file names must starts with 'test_'."
                            " Invalid test names:\n  %s"
                            % '\n  '.join(invalid_tests))

        self.logger.info("running with e10s: {}".format(self.e10s))

        self.logger.suite_start(self.tests)

        for test in self.manifest_skipped_tests:
            name = os.path.basename(test['path'])
            self.logger.test_start(name)
            self.logger.test_end(name,
                                 'SKIP',
                                 message=test['disabled'])
            self.todo += 1

        interrupted = None
        try:
            counter = self.repeat
            while counter >=0:
                round = self.repeat - counter
                if round > 0:
                    self.logger.info('\nREPEAT %d\n-------' % round)
                self.run_test_sets()
                counter -= 1
        except KeyboardInterrupt:
            # in case of KeyboardInterrupt during the test execution
            # we want to display current test results.
            # so we keep the exception to raise it later.
            interrupted = sys.exc_info()
        try:
            self._print_summary(tests)
        except:
            # raise only the exception if we were not interrupted
            if not interrupted:
                raise
        finally:
            # reraise previous interruption now
            if interrupted:
                raise interrupted[0], interrupted[1], interrupted[2]

    def _print_summary(self, tests):
        self.logger.info('\nSUMMARY\n-------')
        self.logger.info('passed: %d' % self.passed)
        if self.unexpected_successes == 0:
            self.logger.info('failed: %d' % self.failed)
        else:
            self.logger.info('failed: %d (unexpected sucesses: %d)' % (self.failed, self.unexpected_successes))
        if self.skipped == 0:
            self.logger.info('todo: %d' % self.todo)
        else:
            self.logger.info('todo: %d (skipped: %d)' % (self.todo, self.skipped))

        if self.failed > 0:
            self.logger.info('\nFAILED TESTS\n-------')
            for failed_test in self.failures:
                self.logger.info('%s' % failed_test[0])

        self.end_time = time.time()
        self.elapsedtime = self.end_time - self.start_time

        for run_tests in self.mixin_run_tests:
            run_tests(tests)
        if self.shuffle:
            self.logger.info("Using seed where seed is:%d" % self.shuffle_seed)

        self.logger.info('mode: {}'.format('e10s' if self.e10s else 'non-e10s'))
        self.logger.suite_end()

    def create_httpd(self, need_external_ip):
        host = "127.0.0.1"
        if need_external_ip:
            host = moznetwork.get_ip()
        root = self.server_root or os.path.join(os.path.dirname(here), "www")
        rv = httpd.FixtureServer(root, host=host)
        rv.start()
        return rv

    def add_test(self, test, expected='pass', test_container=None):
        filepath = os.path.abspath(test)

        if os.path.isdir(filepath):
            for root, dirs, files in os.walk(filepath):
                for filename in files:
                    if (filename.startswith('test_') and
                        (filename.endswith('.py') or filename.endswith('.js'))):
                        filepath = os.path.join(root, filename)
                        self.add_test(filepath)
            return

        file_ext = os.path.splitext(os.path.split(filepath)[-1])[1]

        if file_ext == '.ini':
            manifest = TestManifest()
            manifest.read(filepath)

            filters = []
            if self.test_tags:
                filters.append(tags(self.test_tags))
            manifest_tests = manifest.active_tests(exists=False,
                                                   disabled=True,
                                                   filters=filters,
                                                   e10s=self.e10s,
                                                   **mozinfo.info)
            if len(manifest_tests) == 0:
                self.logger.error("no tests to run using specified "
                                  "combination of filters: {}".format(
                                       manifest.fmt_filters()))

            target_tests = []
            for test in manifest_tests:
                if test.get('disabled'):
                    self.manifest_skipped_tests.append(test)
                else:
                    target_tests.append(test)

            for i in target_tests:
                if not os.path.exists(i["path"]):
                    raise IOError("test file: %s does not exist" % i["path"])

                file_ext = os.path.splitext(os.path.split(i['path'])[-1])[-1]
                test_container = None

                self.add_test(i["path"], i["expected"], test_container)
            return

        self.tests.append({'filepath': filepath, 'expected': expected, 'test_container': test_container})

    def run_test(self, filepath, expected, test_container):

        testloader = unittest.TestLoader()
        suite = unittest.TestSuite()
        self.test_kwargs['binary'] = self.bin
        self.test_kwargs['expected'] = expected
        self.test_kwargs['base_url'] = self.base_url
        self.test_kwargs['test_container'] = test_container
        mod_name = os.path.splitext(os.path.split(filepath)[-1])[0]
        for handler in self.test_handlers:
            if handler.match(os.path.basename(filepath)):
                handler.add_tests_to_suite(mod_name,
                                           filepath,
                                           suite,
                                           testloader,
                                           self.testvars,
                                           **self.test_kwargs)
                break

        if suite.countTestCases():
            runner = self.textrunnerclass(logger=self.logger,
                                          result_callbacks=self.result_callbacks,
                                          binary=self.bin)

            if test_container:
                self.launch_test_container()

            results = runner.run(suite)
            self.results.append(results)

            self.failed += len(results.failures) + len(results.errors)
            if hasattr(results, 'skipped'):
                self.skipped += len(results.skipped)
                self.todo += len(results.skipped)
            self.passed += results.passed
            for failure in results.failures + results.errors:
                self.failures.append((results.getInfo(failure), failure.output, 'TEST-UNEXPECTED-FAIL'))
            if hasattr(results, 'unexpectedSuccesses'):
                self.failed += len(results.unexpectedSuccesses)
                self.unexpected_successes += len(results.unexpectedSuccesses)
                for failure in results.unexpectedSuccesses:
                    self.failures.append((results.getInfo(failure), failure.output, 'TEST-UNEXPECTED-PASS'))
            if hasattr(results, 'expectedFailures'):
                self.todo += len(results.expectedFailures)

            self.mixin_run_tests = []
            for result in self.results:
                result.result_modifiers = []

    def run_test_set(self, tests):
        if self.shuffle:
            random.seed(self.shuffle_seed)
            random.shuffle(tests)

        for test in tests:
            self.run_test(test['filepath'], test['expected'], test['test_container'])

    def run_test_sets(self):
        if len(self.tests) < 1:
            raise Exception('There are no tests to run.')
        elif self.total_chunks > len(self.tests):
            raise ValueError('Total number of chunks must be between 1 and %d.' % len(self.tests))
        if self.total_chunks > 1:
            chunks = [[] for i in range(self.total_chunks)]
            for i, test in enumerate(self.tests):
                target_chunk = i % self.total_chunks
                chunks[target_chunk].append(test)

            self.logger.info('Running chunk %d of %d (%d tests selected from a '
                             'total of %d)' % (self.this_chunk, self.total_chunks,
                                               len(chunks[self.this_chunk - 1]),
                                               len(self.tests)))
            self.tests = chunks[self.this_chunk - 1]

        self.run_test_set(self.tests)

    def cleanup(self):
        if self.httpd:
            self.httpd.stop()

    __del__ = cleanup

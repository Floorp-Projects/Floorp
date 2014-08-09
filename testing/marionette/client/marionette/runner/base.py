# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from optparse import OptionParser

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
import xml.dom.minidom as dom

from manifestparser import TestManifest
from marionette import Marionette
from mixins.b2g import B2GTestResultMixin, get_b2g_pid, get_dm
from mozhttpd import MozHttpd
from moztest.adapters.unit import StructuredTestRunner, StructuredTestResult
from moztest.results import TestResultCollection, TestResult, relevant_line

class MarionetteTest(TestResult):

    @property
    def test_name(self):
        if self.test_class is not None:
            return '%s.py %s.%s' % (self.test_class.split('.')[0],
                                    self.test_class,
                                    self.name)
        else:
            return self.name

class MarionetteTestResult(StructuredTestResult, TestResultCollection):

    resultClass = MarionetteTest

    def __init__(self, *args, **kwargs):
        self.marionette = kwargs.pop('marionette')
        TestResultCollection.__init__(self, 'MarionetteTest')
        self.passed = 0
        self.testsRun = 0
        self.result_modifiers = [] # used by mixins to modify the result
        pid = kwargs.pop('b2g_pid')
        if pid:
            if B2GTestResultMixin not in self.__class__.__bases__:
                bases = [b for b in self.__class__.__bases__]
                bases.append(B2GTestResultMixin)
                self.__class__.__bases__ = tuple(bases)
            B2GTestResultMixin.__init__(self, b2g_pid=pid)
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
        super(MarionetteTestResult, self).addError(test, err)

    def addFailure(self, test, err):
        self.add_test_result(test, output=self._exc_info_to_string(err, test), result_actual='UNEXPECTED-FAIL')
        super(MarionetteTestResult, self).addFailure(test, err)

    def addSuccess(self, test):
        self.passed += 1
        self.add_test_result(test, result_actual='PASS')
        super(MarionetteTestResult, self).addSuccess(test)

    def addExpectedFailure(self, test, err):
        """Called when an expected failure/error occured."""
        self.add_test_result(test, output=self._exc_info_to_string(err, test),
                             result_actual='KNOWN-FAIL')
        super(MarionetteTestResult, self).addExpectedFailure(test, err)

    def addUnexpectedSuccess(self, test):
        """Called when a test was expected to fail, but succeed."""
        self.add_test_result(test, result_actual='UNEXPECTED-PASS')
        super(MarionetteTestResult, self).addUnexpectedSuccess(test)

    def addSkip(self, test, reason):
        self.add_test_result(test, output=reason, result_actual='SKIPPED')
        super(MarionetteTestResult, self).addSkip(test, reason)

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
        if self.marionette.check_for_crash():
            # this tells unittest.TestSuite not to continue running tests
            self.shouldStop = True


class MarionetteTextTestRunner(StructuredTestRunner):

    resultclass = MarionetteTestResult

    def __init__(self, **kwargs):
        self.marionette = kwargs.pop('marionette')
        self.capabilities = kwargs.pop('capabilities')
        self.pre_run_functions = []
        self.b2g_pid = None

        if self.capabilities["device"] != "desktop" and self.capabilities["browserName"] == "B2G":
            def b2g_pre_run():
                dm_type = os.environ.get('DM_TRANS', 'adb')
                if dm_type == 'adb':
                    self.b2g_pid = get_b2g_pid(get_dm(self.marionette))
            self.pre_run_functions.append(b2g_pre_run)

        StructuredTestRunner.__init__(self, **kwargs)


    def _makeResult(self):
        return self.resultclass(self.stream,
                                self.descriptions,
                                self.verbosity,
                                marionette=self.marionette,
                                b2g_pid=self.b2g_pid,
                                logger=self.logger)

    def run(self, test):
        "Run the given test case or test suite."
        for pre_run_func in self.pre_run_functions:
            pre_run_func()

        result = super(MarionetteTextTestRunner, self).run(test)
        result.printLogs(test)
        return result


class BaseMarionetteOptions(OptionParser):
    def __init__(self, **kwargs):
        OptionParser.__init__(self, **kwargs)
        self.parse_args_handlers = [] # Used by mixins
        self.verify_usage_handlers = [] # Used by mixins
        self.add_option('--emulator',
                        action='store',
                        dest='emulator',
                        choices=['x86', 'arm'],
                        help='if no --address is given, then the harness will launch a B2G emulator on which to run '
                             'emulator tests. if --address is given, then the harness assumes you are running an '
                             'emulator already, and will run the emulator tests using that emulator. you need to '
                             'specify which architecture to emulate for both cases')
        self.add_option('--emulator-binary',
                        action='store',
                        dest='emulator_binary',
                        help='launch a specific emulator binary rather than launching from the B2G built emulator')
        self.add_option('--emulator-img',
                        action='store',
                        dest='emulator_img',
                        help='use a specific image file instead of a fresh one')
        self.add_option('--emulator-res',
                        action='store',
                        dest='emulator_res',
                        type='str',
                        help='set a custom resolution for the emulator'
                             'Example: "480x800"')
        self.add_option('--sdcard',
                        action='store',
                        dest='sdcard',
                        help='size of sdcard to create for the emulator')
        self.add_option('--no-window',
                        action='store_true',
                        dest='no_window',
                        default=False,
                        help='when Marionette launches an emulator, start it with the -no-window argument')
        self.add_option('--logcat-dir',
                        dest='logdir',
                        action='store',
                        help='directory to store logcat dump files')
        self.add_option('--address',
                        dest='address',
                        action='store',
                        help='host:port of running Gecko instance to connect to')
        self.add_option('--device',
                        dest='device_serial',
                        action='store',
                        help='serial ID of a device to use for adb / fastboot')
        self.add_option('--type',
                        dest='type',
                        action='store',
                        default='browser+b2g',
                        help="the type of test to run, can be a combination of values defined in the manifest file; "
                             "individual values are combined with '+' or '-' characters. for example: 'browser+b2g' "
                             "means the set of tests which are compatible with both browser and b2g; 'b2g-qemu' means "
                             "the set of tests which are compatible with b2g but do not require an emulator. this "
                             "argument is only used when loading tests from manifest files")
        self.add_option('--homedir',
                        dest='homedir',
                        action='store',
                        help='home directory of emulator files')
        self.add_option('--app',
                        dest='app',
                        action='store',
                        help='application to use')
        self.add_option('--app-arg',
                        dest='app_args',
                        action='append',
                        default=[],
                        help='specify a command line argument to be passed onto the application')
        self.add_option('--binary',
                        dest='binary',
                        action='store',
                        help='gecko executable to launch before running the test')
        self.add_option('--profile',
                        dest='profile',
                        action='store',
                        help='profile to use when launching the gecko process. if not passed, then a profile will be '
                             'constructed and used')
        self.add_option('--repeat',
                        dest='repeat',
                        action='store',
                        type=int,
                        default=0,
                        help='number of times to repeat the test(s)')
        self.add_option('-x', '--xml-output',
                        action='store',
                        dest='xml_output',
                        help='xml output')
        self.add_option('--testvars',
                        dest='testvars',
                        action='store',
                        help='path to a json file with any test data required')
        self.add_option('--tree',
                        dest='tree',
                        action='store',
                        default='b2g',
                        help='the tree that the revision parameter refers to')
        self.add_option('--symbols-path',
                        dest='symbols_path',
                        action='store',
                        help='absolute path to directory containing breakpad symbols, or the url of a zip file containing symbols')
        self.add_option('--timeout',
                        dest='timeout',
                        type=int,
                        help='if a --timeout value is given, it will set the default page load timeout, search timeout and script timeout to the given value. If not passed in, it will use the default values of 30000ms for page load, 0ms for search timeout and 10000ms for script timeout')
        self.add_option('--shuffle',
                        action='store_true',
                        dest='shuffle',
                        default=False,
                        help='run tests in a random order')
        self.add_option('--shuffle-seed',
                        dest='shuffle_seed',
                        type=int,
                        default=random.randint(0, sys.maxint),
                        help='Use given seed to shuffle tests')
        self.add_option('--total-chunks',
                        dest='total_chunks',
                        type=int,
                        help='how many chunks to split the tests up into')
        self.add_option('--this-chunk',
                        dest='this_chunk',
                        type=int,
                        help='which chunk to run')
        self.add_option('--sources',
                        dest='sources',
                        action='store',
                        help='path to sources.xml (Firefox OS only)')
        self.add_option('--server-root',
                        dest='server_root',
                        action='store',
                        help='sets the web server\'s root directory to the given path')
        self.add_option('--gecko-log',
                        dest='gecko_log',
                        action='store',
                        help="Define the path to store log file. If the path is"
                             " a directory, the real log file will be created"
                             " given the format gecko-(timestamp).log. If it is"
                             " a file, if will be used directly. Default: 'gecko.log'")
        self.add_option('--logger-name',
                        dest='logger_name',
                        action='store',
                        default='Marionette-based Tests',
                        help='Define the name to associate with the logger used')


    def parse_args(self, args=None, values=None):
        options, tests = OptionParser.parse_args(self, args, values)
        for handler in self.parse_args_handlers:
            handler(options, tests, args, values)

        return (options, tests)

    def verify_usage(self, options, tests):
        if not tests:
            print 'must specify one or more test files, manifests, or directories'
            sys.exit(1)

        if not options.emulator and not options.address and not options.binary:
            print 'must specify --binary, --emulator or --address'
            sys.exit(1)

        if options.emulator and options.binary:
            print 'can\'t specify both --emulator and --binary'
            sys.exit(1)

        # default to storing logcat output for emulator runs
        if options.emulator and not options.logdir:
            options.logdir = 'logcat'

        # check for valid resolution string, strip whitespaces
        try:
            if options.emulator_res:
                dims = options.emulator_res.split('x')
                assert len(dims) == 2
                width = str(int(dims[0]))
                height = str(int(dims[1]))
                options.emulator_res = 'x'.join([width, height])
        except:
            raise ValueError('Invalid emulator resolution format. '
                             'Should be like "480x800".')

        if options.total_chunks is not None and options.this_chunk is None:
            self.error('You must specify which chunk to run.')

        if options.this_chunk is not None and options.total_chunks is None:
            self.error('You must specify how many chunks to split the tests into.')

        if options.total_chunks is not None:
            if not 1 <= options.total_chunks:
                self.error('Total chunks must be greater than 1.')
            if not 1 <= options.this_chunk <= options.total_chunks:
                self.error('Chunk to run must be between 1 and %s.' % options.total_chunks)

        for handler in self.verify_usage_handlers:
            handler(options, tests)

        return (options, tests)


class BaseMarionetteTestRunner(object):

    textrunnerclass = MarionetteTextTestRunner

    def __init__(self, address=None, emulator=None, emulator_binary=None,
                 emulator_img=None, emulator_res='480x800', homedir=None,
                 app=None, app_args=None, binary=None, profile=None,
                 logger=None, no_window=False, logdir=None, xml_output=None,
                 repeat=0, testvars=None, tree=None, type=None,
                 device_serial=None, symbols_path=None, timeout=None,
                 shuffle=False, shuffle_seed=random.randint(0, sys.maxint),
                 sdcard=None, this_chunk=1, total_chunks=1, sources=None,
                 server_root=None, gecko_log=None, **kwargs):
        self.address = address
        self.emulator = emulator
        self.emulator_binary = emulator_binary
        self.emulator_img = emulator_img
        self.emulator_res = emulator_res
        self.homedir = homedir
        self.app = app
        self.app_args = app_args or []
        self.bin = binary
        self.profile = profile
        self.logger = logger
        self.no_window = no_window
        self.httpd = None
        self.marionette = None
        self.logdir = logdir
        self.xml_output = xml_output
        self.repeat = repeat
        self.testvars = {}
        self.test_kwargs = kwargs
        self.tree = tree
        self.type = type
        self.device_serial = device_serial
        self.symbols_path = symbols_path
        self.timeout = timeout
        self._device = None
        self._capabilities = None
        self._appName = None
        self.shuffle = shuffle
        self.shuffle_seed = shuffle_seed
        self.sdcard = sdcard
        self.sources = sources
        self.server_root = server_root
        self.this_chunk = this_chunk
        self.total_chunks = total_chunks
        self.gecko_log = gecko_log
        self.mixin_run_tests = []
        self.manifest_skipped_tests = []
        self.tests = []

        if testvars:
            if not os.path.exists(testvars):
                raise IOError('--testvars file does not exist')

            try:
                with open(testvars) as f:
                    self.testvars = json.loads(f.read())
            except ValueError as e:
                json_path = os.path.abspath(testvars)
                raise Exception("JSON file (%s) is not properly "
                                "formatted: %s" % (json_path, e.message))

        # set up test handlers
        self.test_handlers = []

        self.reset_test_stats()

        if self.logdir:
            if not os.access(self.logdir, os.F_OK):
                os.mkdir(self.logdir)

        # for XML output
        self.testvars['xml_output'] = self.xml_output
        self.results = []

    @property
    def capabilities(self):
        if self._capabilities:
            return self._capabilities

        self.marionette.start_session()
        self._capabilities = self.marionette.session_capabilities
        self.marionette.delete_session()
        return self._capabilities

    @property
    def device(self):
        if self._device:
            return self._device

        self._device = self.capabilities.get('device')
        return self._device

    @property
    def appName(self):
        if self._appName:
            return self._appName

        self._appName = self.capabilities.get('browserName')
        return self._appName

    def reset_test_stats(self):
        self.passed = 0
        self.failed = 0
        self.unexpected_successes = 0
        self.todo = 0
        self.skipped = 0
        self.failures = []

    def start_httpd(self, need_external_ip):
        host = "127.0.0.1"
        if need_external_ip:
            host = moznetwork.get_ip()
        docroot = self.server_root or os.path.join(os.path.dirname(os.path.dirname(__file__)), 'www')
        if not os.path.isdir(docroot):
            raise Exception("Server root %s is not a valid path" % docroot)
        self.httpd = MozHttpd(host=host,
                              port=0,
                              docroot=docroot)
        self.httpd.start()
        self.marionette.baseurl = 'http://%s:%d/' % (host, self.httpd.httpd.server_port)
        self.logger.info('running webserver on %s' % self.marionette.baseurl)


    def _build_kwargs(self):
        kwargs = {
            'device_serial': self.device_serial,
            'symbols_path': self.symbols_path,
            'timeout': self.timeout,
        }
        if self.bin:
            kwargs.update({
                'host': 'localhost',
                'port': 2828,
                'app': self.app,
                'app_args': self.app_args,
                'bin': self.bin,
                'profile': self.profile,
                'gecko_log': self.gecko_log,
            })

        if self.emulator:
            kwargs.update({
                'homedir': self.homedir,
                'logdir': self.logdir,
            })

        if self.address:
            host, port = self.address.split(':')
            kwargs.update({
                'host': host,
                'port': int(port),
            })
            if self.emulator:
                kwargs['connectToRunningEmulator'] = True

            if not self.bin:
                try:
                    #establish a socket connection so we can vertify the data come back
                    connection = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
                    connection.connect((host,int(port)))
                    connection.close()
                except Exception, e:
                    raise Exception("Connection attempt to %s:%s failed with error: %s" %(host,port,e))
        elif self.emulator:
            kwargs.update({
                'emulator': self.emulator,
                'emulator_binary': self.emulator_binary,
                'emulator_img': self.emulator_img,
                'emulator_res': self.emulator_res,
                'no_window': self.no_window,
                'sdcard': self.sdcard,
            })
        return kwargs

    def start_marionette(self):
        self.marionette = Marionette(**self._build_kwargs())

    def run_tests(self, tests):
        self.reset_test_stats()
        self.start_time = time.time()

        need_external_ip = True
        if not self.marionette:
            self.start_marionette()
            if self.emulator:
                self.marionette.emulator.wait_for_homescreen(self.marionette)
            # Retrieve capabilities for later use
            if not self._capabilities:
                self.capabilities
            # if we're working against a desktop version, we usually don't need
            # an external ip
            if self._capabilities['device'] == "desktop":
                need_external_ip = False

        if not self.httpd:
            self.logger.info("starting httpd")
            self.start_httpd(need_external_ip)

        for test in tests:
            self.add_test(test)

        self.logger.suite_start(self.tests)

        for test in self.manifest_skipped_tests:
            name = os.path.basename(test['path'])
            self.logger.test_start(name)
            self.logger.test_end(name,
                                 'SKIP',
                                 message=test['disabled'])
            self.todo += 1

        counter = self.repeat
        while counter >=0:
            round = self.repeat - counter
            if round > 0:
                self.logger.info('\nREPEAT %d\n-------' % round)
            self.run_test_sets()
            counter -= 1

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

        try:
            self.marionette.check_for_crash()
        except:
            traceback.print_exc()

        self.end_time = time.time()
        self.elapsedtime = self.end_time - self.start_time

        if self.xml_output:
            xml_dir = os.path.dirname(os.path.abspath(self.xml_output))
            if not os.path.exists(xml_dir):
                os.makedirs(xml_dir)
            with open(self.xml_output, 'w') as f:
                f.write(self.generate_xml(self.results))

        if self.marionette.instance:
            self.marionette.instance.close()
            self.marionette.instance = None

        self.marionette.cleanup()

        for run_tests in self.mixin_run_tests:
            run_tests(tests)
        if self.shuffle:
            self.logger.info("Using seed where seed is:%d" % self.shuffle_seed)

        self.logger.suite_end()

    def add_test(self, test, expected='pass', oop=None):
        filepath = os.path.abspath(test)

        if os.path.isdir(filepath):
            for root, dirs, files in os.walk(filepath):
                for filename in files:
                    if (filename.startswith('test_') and
                        (filename.endswith('.py') or filename.endswith('.js'))):
                        filepath = os.path.join(root, filename)
                        self.add_test(filepath)
            return

        testargs = {}
        if self.type is not None:
            testtypes = self.type.replace('+', ' +').replace('-', ' -').split()
            for atype in testtypes:
                if atype.startswith('+'):
                    testargs.update({ atype[1:]: 'true' })
                elif atype.startswith('-'):
                    testargs.update({ atype[1:]: 'false' })
                else:
                    testargs.update({ atype: 'true' })

        # testarg_oop = either None, 'true' or 'false'.
        testarg_oop = testargs.get('oop')

        file_ext = os.path.splitext(os.path.split(filepath)[-1])[1]

        if file_ext == '.ini':
            manifest = TestManifest()
            manifest.read(filepath)

            manifest_tests = manifest.active_tests(exists=False,
                                                   disabled=True,
                                                   device=self.device,
                                                   app=self.appName,
                                                   **mozinfo.info)
            unfiltered_tests = []
            for test in manifest_tests:
                if test.get('disabled'):
                    self.manifest_skipped_tests.append(test)
                else:
                    unfiltered_tests.append(test)

            # Don't filter tests with "oop" flag because manifest parser can't
            # handle it well.
            if testarg_oop is not None:
                del testargs['oop']

            target_tests = manifest.get(tests=unfiltered_tests, **testargs)
            for test in unfiltered_tests:
                if test['path'] not in [x['path'] for x in target_tests]:
                    test.setdefault('disabled', 'filtered by type (%s)' % self.type)
                    self.manifest_skipped_tests.append(test)

            for i in target_tests:
                if not os.path.exists(i["path"]):
                    raise IOError("test file: %s does not exist" % i["path"])

                # manifest_oop is either 'false', 'true' or 'both'.  Anything
                # else implies 'false'.
                manifest_oop = i.get('oop', 'false')

                # We only add an oop test when following conditions are met:
                # 1) It's written by javascript because we have only
                #    MarionetteJSTestCase that supports oop mode.
                # 2) we're running with "--type=+oop" or no "--type=-oop", which
                #    follows testarg_oop is either None or 'true' and must not
                #    be 'false'.
                # 3) When no "--type=[+-]oop" is applied, all active tests are
                #    included in target_tests, so we must filter out those
                #    really capable of running in oop mode. Besides, oop tests
                #    must be explicitly specified for backward compatibility. So
                #    test manifest_oop equals to either 'both' or 'true'.
                file_ext = os.path.splitext(os.path.split(i['path'])[-1])[-1]
                if (file_ext == '.js' and
                    testarg_oop != 'false' and
                    (manifest_oop == 'both' or manifest_oop == 'true')):
                    self.add_test(i["path"], i["expected"], True)

                # We only add an in-process test when following conditions are
                # met:
                # 1) we're running with "--type=-oop" or no "--type=+oop", which
                #    follows testarg_oop is either None or 'false' and must not
                #    be 'true'.
                # 2) When no "--type=[+-]oop" is applied, all active tests are
                #    included in target_tests, so we must filter out those
                #    really capable of running in in-process mode.
                if (testarg_oop != 'true' and
                    (manifest_oop == 'both' or manifest_oop != 'true')):
                    self.add_test(i["path"], i["expected"], False)
            return

        if oop is None:
            # This test is added by directory enumeration or directly specified
            # in argument list.  We have no manifest information here so we just
            # respect the "--type=[+-]oop" argument here.
            oop = file_ext == '.js' and testarg_oop == 'true'

        self.tests.append({'filepath': filepath, 'expected': expected, 'oop': oop})

    def run_test(self, filepath, expected, oop):

        testloader = unittest.TestLoader()
        suite = unittest.TestSuite()
        self.test_kwargs['expected'] = expected
        self.test_kwargs['oop'] = oop
        mod_name = os.path.splitext(os.path.split(filepath)[-1])[0]
        for handler in self.test_handlers:
            if handler.match(os.path.basename(filepath)):
                handler.add_tests_to_suite(mod_name,
                                           filepath,
                                           suite,
                                           testloader,
                                           self.marionette,
                                           self.testvars,
                                           **self.test_kwargs)
                break

        if suite.countTestCases():
            runner = self.textrunnerclass(logger=self.logger,
                                          marionette=self.marionette,
                                          capabilities=self.capabilities)
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

    def run_test_set(self, tests):
        if self.shuffle:
            random.seed(self.shuffle_seed)
            random.shuffle(tests)

        for test in tests:
            self.run_test(test['filepath'], test['expected'], test['oop'])
            if self.marionette.check_for_crash():
                break

    def run_test_sets(self):
        if self.total_chunks > len(self.tests):
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

        oop_tests = [x for x in self.tests if x.get('oop')]
        self.run_test_set(oop_tests)

        in_process_tests = [x for x in self.tests if not x.get('oop')]
        self.run_test_set(in_process_tests)

    def cleanup(self):
        if self.httpd:
            self.httpd.stop()

    __del__ = cleanup

    def generate_xml(self, results_list):

        def _extract_xml_from_result(test_result, result='passed'):
            _extract_xml(
                test_name=unicode(test_result.name).split()[0],
                test_class=test_result.test_class,
                duration=test_result.duration,
                result=result,
                output='\n'.join(test_result.output))

        def _extract_xml_from_skipped_manifest_test(test):
            _extract_xml(
                test_name=test['name'],
                result='skipped',
                output=test['disabled'])

        def _extract_xml(test_name, test_class='', duration=0,
                         result='passed', output=''):
            testcase = doc.createElement('testcase')
            testcase.setAttribute('classname', test_class)
            testcase.setAttribute('name', test_name)
            testcase.setAttribute('time', str(duration))
            testsuite.appendChild(testcase)

            if result in ['failure', 'error', 'skipped']:
                f = doc.createElement(result)
                f.setAttribute('message', 'test %s' % result)
                f.appendChild(doc.createTextNode(output))
                testcase.appendChild(f)

        doc = dom.Document()

        testsuite = doc.createElement('testsuite')
        testsuite.setAttribute('name', 'Marionette')
        testsuite.setAttribute('time', str(self.elapsedtime))
        testsuite.setAttribute('tests', str(sum([results.testsRun for
                                                 results in results_list])))

        def failed_count(results):
            count = len(results.failures)
            if hasattr(results, 'unexpectedSuccesses'):
                count += len(results.unexpectedSuccesses)
            return count

        testsuite.setAttribute('failures', str(sum([failed_count(results)
                                               for results in results_list])))
        testsuite.setAttribute('errors', str(sum([len(results.errors)
                                             for results in results_list])))
        testsuite.setAttribute(
            'skips', str(sum([len(results.skipped) +
                         len(results.expectedFailures)
                         for results in results_list]) +
                         len(self.manifest_skipped_tests)))

        for results in results_list:

            for result in results.errors:
                _extract_xml_from_result(result, result='error')

            for result in results.failures:
                _extract_xml_from_result(result, result='failure')

            if hasattr(results, 'unexpectedSuccesses'):
                for test in results.unexpectedSuccesses:
                    # unexpectedSuccesses is a list of Testcases only, no tuples
                    _extract_xml_from_result(test, result='failure')

            if hasattr(results, 'skipped'):
                for result in results.skipped:
                    _extract_xml_from_result(result, result='skipped')

            if hasattr(results, 'expectedFailures'):
                for result in results.expectedFailures:
                    _extract_xml_from_result(result, result='skipped')

            for result in results.tests_passed:
                _extract_xml_from_result(result)

        for test in self.manifest_skipped_tests:
            _extract_xml_from_skipped_manifest_test(test)

        doc.appendChild(testsuite)
        return doc.toprettyxml(encoding='utf-8')

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from optparse import OptionParser
from datetime import datetime
import logging
import os
import unittest
import socket
import sys
import time
import traceback
import random
import mozinfo
import moznetwork
import xml.dom.minidom as dom

from manifestparser import TestManifest
from mozhttpd import MozHttpd
from marionette import Marionette
from moztest.results import TestResultCollection, TestResult, relevant_line
from mixins.b2g import B2GTestResultMixin, get_b2g_pid, get_dm


class MarionetteTest(TestResult):

    @property
    def test_name(self):
        if self.test_class is not None:
            return '%s.py %s.%s' % (self.test_class.split('.')[0],
                                    self.test_class,
                                    self.name)
        else:
            return self.name


class MarionetteTestResult(unittest._TextTestResult, TestResultCollection):

    resultClass = MarionetteTest

    def __init__(self, *args, **kwargs):
        self.marionette = kwargs.pop('marionette')
        TestResultCollection.__init__(self, 'MarionetteTest')
        unittest._TextTestResult.__init__(self, *args, **kwargs)
        self.passed = 0
        self.testsRun = 0
        self.result_modifiers = [] # used by mixins to modify the result

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
        self._mirrorOutput = True
        if self.showAll:
            self.stream.writeln("ERROR")
        elif self.dots:
            self.stream.write('E')
            self.stream.flush()

    def addFailure(self, test, err):
        self.add_test_result(test, output=self._exc_info_to_string(err, test), result_actual='UNEXPECTED-FAIL')
        self._mirrorOutput = True
        if self.showAll:
            self.stream.writeln("FAIL")
        elif self.dots:
            self.stream.write('F')
            self.stream.flush()

    def addSuccess(self, test):
        self.passed += 1
        self.add_test_result(test, result_actual='PASS')
        if self.showAll:
            self.stream.writeln("ok")
        elif self.dots:
            self.stream.write('.')
            self.stream.flush()

    def addExpectedFailure(self, test, err):
        """Called when an expected failure/error occured."""
        self.add_test_result(test, output=self._exc_info_to_string(err, test),
                        result_actual='KNOWN-FAIL')
        if self.showAll:
            self.stream.writeln("expected failure")
        elif self.dots:
            self.stream.write("x")
            self.stream.flush()

    def addUnexpectedSuccess(self, test):
        """Called when a test was expected to fail, but succeed."""
        self.add_test_result(test, result_actual='UNEXPECTED-PASS')
        if self.showAll:
            self.stream.writeln("unexpected success")
        elif self.dots:
            self.stream.write("u")
            self.stream.flush()

    def addSkip(self, test, reason):
        self.add_test_result(test, output=reason, result_actual='SKIPPED')
        if self.showAll:
            self.stream.writeln("skipped {0!r}".format(reason))
        elif self.dots:
            self.stream.write("s")
            self.stream.flush()

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
                self.stream.writeln('\nSTART LOG:')
                for line in testcase.loglines:
                    self.stream.writeln(' '.join(line).encode('ascii', 'replace'))
                self.stream.writeln('END LOG:')

    def printErrorList(self, flavour, errors):
        for error in errors:
            err = error.output
            self.stream.writeln(self.separator1)
            self.stream.writeln("%s: %s" % (flavour, error.description))
            self.stream.writeln(self.separator2)
            lastline = None
            fail_present = None
            for line in err:
                if not line.startswith('\t'):
                    lastline = line
                if 'TEST-UNEXPECTED-FAIL' in line:
                    fail_present = True
            for line in err:
                if line != lastline or fail_present:
                    self.stream.writeln("%s" % line)
                else:
                    self.stream.writeln("TEST-UNEXPECTED-FAIL | %s | %s" %
                                        (self.getInfo(error), line))

    def stopTest(self, *args, **kwargs):
        unittest._TextTestResult.stopTest(self, *args, **kwargs)
        if self.marionette.check_for_crash():
            # this tells unittest.TestSuite not to continue running tests
            self.shouldStop = True


class MarionetteTextTestRunner(unittest.TextTestRunner):

    resultclass = MarionetteTestResult

    def __init__(self, **kwargs):
        self.marionette = kwargs['marionette']
        self.capabilities = kwargs.pop('capabilities')
        del kwargs['marionette']
        unittest.TextTestRunner.__init__(self, **kwargs)

    def _makeResult(self):
        return self.resultclass(self.stream,
                                self.descriptions,
                                self.verbosity,
                                marionette=self.marionette)

    def run(self, test):
        "Run the given test case or test suite."
        result = self._makeResult()
        if hasattr(self, 'failfast'):
            result.failfast = self.failfast
        if hasattr(self, 'buffer'):
            result.buffer = self.buffer
        startTime = time.time()
        startTestRun = getattr(result, 'startTestRun', None)
        if startTestRun is not None:
            startTestRun()
        try:
            test(result)
        finally:
            stopTestRun = getattr(result, 'stopTestRun', None)
            if stopTestRun is not None:
                stopTestRun()
        stopTime = time.time()
        if hasattr(result, 'time_taken'):
            result.time_taken = stopTime - startTime
        result.printLogs(test)
        result.printErrors()
        if hasattr(result, 'separator2'):
            self.stream.writeln(result.separator2)
        run = result.testsRun
        self.stream.writeln("Ran %d test%s in %.3fs" %
                            (run, run != 1 and "s" or "", result.time_taken))
        self.stream.writeln()

        expectedFails = unexpectedSuccesses = skipped = 0
        try:
            results = map(len, (result.expectedFailures,
                                result.unexpectedSuccesses,
                                result.skipped))
        except AttributeError:
            pass
        else:
            expectedFails, unexpectedSuccesses, skipped = results

        infos = []
        if not result.wasSuccessful():
            self.stream.write("FAILED")
            failed, errored = map(len, (result.failures, result.errors))
            if failed:
                infos.append("failures=%d" % failed)
            if errored:
                infos.append("errors=%d" % errored)
        else:
            self.stream.write("OK")
        if skipped:
            infos.append("skipped=%d" % skipped)
        if expectedFails:
            infos.append("expected failures=%d" % expectedFails)
        if unexpectedSuccesses:
            infos.append("unexpected successes=%d" % unexpectedSuccesses)
        if infos:
            self.stream.writeln(" (%s)" % (", ".join(infos),))
        else:
            self.stream.write("\n")
        return result


class B2GMarionetteTestResult(MarionetteTestResult, B2GTestResultMixin):

    def __init__(self, *args, **kwargs):
        # stupid hack because _TextTestRunner doesn't accept **kwargs
        b2g_pid = kwargs.pop('b2g_pid')
        MarionetteTestResult.__init__(self, *args, **kwargs)
        kwargs['b2g_pid'] = b2g_pid
        B2GTestResultMixin.__init__(self, *args, **kwargs)
 

class B2GMarionetteTextTestRunner(MarionetteTextTestRunner):

    resultclass = B2GMarionetteTestResult

    def __init__(self, **kwargs):
        MarionetteTextTestRunner.__init__(self, **kwargs)
        if self.capabilities['device'] != 'desktop':
            self.resultclass = B2GMarionetteTestResult
        self.b2g_pid = None

    def _makeResult(self):
        return self.resultclass(self.stream,
                                self.descriptions,
                                self.verbosity,
                                marionette=self.marionette,
                                b2g_pid=self.b2g_pid)

    def run(self, test):
        dm_type = os.environ.get('DM_TRANS', 'adb')
        if dm_type == 'adb':
            self.b2g_pid = get_b2g_pid(get_dm(self.marionette))
        return super(B2GMarionetteTextTestRunner, self).run(test)


class BaseMarionetteOptions(OptionParser):
    def __init__(self, **kwargs):
        OptionParser.__init__(self, **kwargs)
        self.parse_args_handlers = [] # Used by mixins
        self.verify_usage_handlers = [] # Used by mixins
        self.add_option('--autolog',
                        action='store_true',
                        dest='autolog',
                        default=False,
                        help='send test results to autolog')
        self.add_option('--revision',
                        action='store',
                        dest='revision',
                        help='git revision for autolog submissions')
        self.add_option('--testgroup',
                        action='store',
                        dest='testgroup',
                        help='testgroup names for autolog submissions')
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
                        dest='emulatorBinary',
                        help='launch a specific emulator binary rather than launching from the B2G built emulator')
        self.add_option('--emulator-img',
                        action='store',
                        dest='emulatorImg',
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
                        dest='noWindow',
                        default=False,
                        help='when Marionette launches an emulator, start it with the -no-window argument')
        self.add_option('--logcat-dir',
                        dest='logcat_dir',
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
                        dest='bin',
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
        self.add_option('--gecko-path',
                        dest='gecko_path',
                        action='store',
                        help='path to b2g gecko binaries that should be installed on the device or emulator')
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
        self.add_option('--es-server',
                        dest='es_servers',
                        action='append',
                        help='the ElasticSearch server to use for autolog submission')
        self.add_option('--shuffle',
                        action='store_true',
                        dest='shuffle',
                        default=False,
                        help='run tests in a random order')

    def parse_args(self, args=None, values=None):
        options, tests = OptionParser.parse_args(self, args, values)
        for handler in self.parse_args_handlers:
            handler(options, tests, args, values)

        return (options, tests)

    def verify_usage(self, options, tests):
        if not tests:
            print 'must specify one or more test files, manifests, or directories'
            sys.exit(1)

        if not options.emulator and not options.address and not options.bin:
            print 'must specify --binary, --emulator or --address'
            sys.exit(1)

        if not options.es_servers:
            options.es_servers = ['elasticsearch-zlb.dev.vlan81.phx.mozilla.com:9200',
                                  'elasticsearch-zlb.webapp.scl3.mozilla.com:9200']

        # default to storing logcat output for emulator runs
        if options.emulator and not options.logcat_dir:
            options.logcat_dir = 'logcat'

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

        for handler in self.verify_usage_handlers:
            handler(options, tests)

        return (options, tests)


class BaseMarionetteTestRunner(object):

    textrunnerclass = MarionetteTextTestRunner

    def __init__(self, address=None, emulator=None, emulatorBinary=None,
                 emulatorImg=None, emulator_res='480x800', homedir=None,
                 app=None, app_args=None, bin=None, profile=None, autolog=False,
                 revision=None, logger=None, testgroup="marionette", noWindow=False,
                 logcat_dir=None, xml_output=None, repeat=0, gecko_path=None,
                 testvars=None, tree=None, type=None, device_serial=None,
                 symbols_path=None, timeout=None, es_servers=None, shuffle=False,
                 sdcard=None, **kwargs):
        self.address = address
        self.emulator = emulator
        self.emulatorBinary = emulatorBinary
        self.emulatorImg = emulatorImg
        self.emulator_res = emulator_res
        self.homedir = homedir
        self.app = app
        self.app_args = app_args or []
        self.bin = bin
        self.profile = profile
        self.autolog = autolog
        self.testgroup = testgroup
        self.revision = revision
        self.logger = logger
        self.noWindow = noWindow
        self.httpd = None
        self.baseurl = None
        self.marionette = None
        self.logcat_dir = logcat_dir
        self.xml_output = xml_output
        self.repeat = repeat
        self.gecko_path = gecko_path
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
        self.es_servers = es_servers
        self.shuffle = shuffle
        self.sdcard = sdcard
        self.mixin_run_tests = []
        self.manifest_skipped_tests = []

        if testvars:
            if not os.path.exists(testvars):
                raise IOError('--testvars file does not exist')

            import json
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

        if self.logger is None:
            self.logger = logging.getLogger('Marionette')
            self.logger.setLevel(logging.INFO)
            self.logger.addHandler(logging.StreamHandler())

        if self.logcat_dir:
            if not os.access(self.logcat_dir, os.F_OK):
                os.mkdir(self.logcat_dir)

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
        self.todo = 0
        self.failures = []

    def start_httpd(self):
        host = moznetwork.get_ip()
        self.httpd = MozHttpd(host=host,
                              port=0,
                              docroot=os.path.join(os.path.dirname(os.path.dirname(__file__)), 'www'))
        self.httpd.start()
        self.baseurl = 'http://%s:%d/' % (host, self.httpd.httpd.server_port)
        self.logger.info('running webserver on %s' % self.baseurl)

    def start_marionette(self):
        assert(self.baseurl is not None)
        if self.bin:
            if self.address:
                host, port = self.address.split(':')
            else:
                host = 'localhost'
                port = 2828
            self.marionette = Marionette(host=host,
                                         port=int(port),
                                         app=self.app,
                                         app_args=self.app_args,
                                         bin=self.bin,
                                         profile=self.profile,
                                         baseurl=self.baseurl,
                                         timeout=self.timeout,
                                         device_serial=self.device_serial)
        elif self.address:
            host, port = self.address.split(':')
            try:
                #establish a socket connection so we can vertify the data come back
                connection = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
                connection.connect((host,int(port)))
                connection.close()
            except Exception, e:
                raise Exception("Could not connect to given marionette host:port: %s" % e)
            if self.emulator:
                self.marionette = Marionette.getMarionetteOrExit(
                                             host=host, port=int(port),
                                             connectToRunningEmulator=True,
                                             homedir=self.homedir,
                                             baseurl=self.baseurl,
                                             logcat_dir=self.logcat_dir,
                                             gecko_path=self.gecko_path,
                                             symbols_path=self.symbols_path,
                                             timeout=self.timeout,
                                             device_serial=self.device_serial)
            else:
                self.marionette = Marionette(host=host,
                                             port=int(port),
                                             baseurl=self.baseurl,
                                             timeout=self.timeout,
                                             device_serial=self.device_serial)
        elif self.emulator:
            self.marionette = Marionette.getMarionetteOrExit(
                                         emulator=self.emulator,
                                         emulatorBinary=self.emulatorBinary,
                                         emulatorImg=self.emulatorImg,
                                         emulator_res=self.emulator_res,
                                         homedir=self.homedir,
                                         baseurl=self.baseurl,
                                         noWindow=self.noWindow,
                                         logcat_dir=self.logcat_dir,
                                         gecko_path=self.gecko_path,
                                         symbols_path=self.symbols_path,
                                         timeout=self.timeout,
                                         sdcard=self.sdcard,
                                         device_serial=self.device_serial)
        else:
            raise Exception("must specify binary, address or emulator")

    def post_to_autolog(self, elapsedtime):
        self.logger.info('posting results to autolog')

        logfile = None
        if self.emulator:
            filename = os.path.join(os.path.abspath(self.logcat_dir),
                                    "emulator-%d.log" % self.marionette.emulator.port)
            if os.access(filename, os.F_OK):
                logfile = filename

        for es_server in self.es_servers:

            # This is all autolog stuff.
            # See: https://wiki.mozilla.org/Auto-tools/Projects/Autolog
            from mozautolog import RESTfulAutologTestGroup
            testgroup = RESTfulAutologTestGroup(
                testgroup=self.testgroup,
                os='android',
                platform='emulator',
                harness='marionette',
                server=es_server,
                restserver=None,
                machine=socket.gethostname(),
                logfile=logfile)

            testgroup.set_primary_product(
                tree=self.tree,
                buildtype='opt',
                revision=self.revision)

            testgroup.add_test_suite(
                testsuite='b2g emulator testsuite',
                elapsedtime=elapsedtime.seconds,
                cmdline='',
                passed=self.passed,
                failed=self.failed,
                todo=self.todo)

            # Add in the test failures.
            for f in self.failures:
                testgroup.add_test_failure(test=f[0], text=f[1], status=f[2])

            testgroup.submit()

    def run_tests(self, tests):
        self.reset_test_stats()
        starttime = datetime.utcnow()
        counter = self.repeat
        while counter >=0:
            round = self.repeat - counter
            if round > 0:
                self.logger.info('\nREPEAT %d\n-------' % round)
            if self.shuffle:
                random.shuffle(tests)
            for test in tests:
                self.run_test(test)
            counter -= 1
        self.logger.info('\nSUMMARY\n-------')
        self.logger.info('passed: %d' % self.passed)
        self.logger.info('failed: %d' % self.failed)
        self.logger.info('todo: %d' % self.todo)

        if self.failed > 0:
            self.logger.info('\nFAILED TESTS\n-------')
            for failed_test in self.failures:
                self.logger.info('%s' % failed_test[0])

        try:
            self.marionette.check_for_crash()
        except:
            traceback.print_exc()

        self.elapsedtime = datetime.utcnow() - starttime
        if self.autolog:
            self.post_to_autolog(self.elapsedtime)

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

    def run_test(self, test, expected='pass'):
        if not self.httpd:
            print "starting httpd"
            self.start_httpd()

        if not self.marionette:
            self.start_marionette()
            if self.emulator:
                self.marionette.emulator.wait_for_homescreen(self.marionette)
            # Retrieve capabilities for later use
            if not self._capabilities:
                self.capabilities

        if self.capabilities['device'] != 'desktop':
            self.textrunnerclass = B2GMarionetteTextTestRunner

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
        oop = testargs.get('oop', False)
        if isinstance(oop, basestring):
            oop = False if oop == 'false' else 'true'

        filepath = os.path.abspath(test)

        if os.path.isdir(filepath):
            for root, dirs, files in os.walk(filepath):
                if self.shuffle:
                    random.shuffle(files)
                for filename in files:
                    if ((filename.startswith('test_') or filename.startswith('browser_')) and
                        (filename.endswith('.py') or filename.endswith('.js'))):
                        filepath = os.path.join(root, filename)
                        self.run_test(filepath)
                        if self.marionette.check_for_crash():
                            return
            return

        mod_name,file_ext = os.path.splitext(os.path.split(filepath)[-1])

        testloader = unittest.TestLoader()
        suite = unittest.TestSuite()

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

            target_tests = manifest.get(tests=unfiltered_tests, **testargs)
            for test in unfiltered_tests:
                if test['path'] not in [x['path'] for x in target_tests]:
                    test.setdefault('disabled', 'filtered by type (%s)' % self.type)
                    self.manifest_skipped_tests.append(test)

            for test in self.manifest_skipped_tests:
                self.logger.info('TEST-SKIP | %s | %s' % (
                    os.path.basename(test['path']),
                    test['disabled']))
                self.todo += 1

            if self.shuffle:
                random.shuffle(target_tests)
            for i in target_tests:
                if not os.path.exists(i["path"]):
                    raise IOError("test file: %s does not exist" % i["path"])
                self.run_test(i["path"], i["expected"])
                if self.marionette.check_for_crash():
                    return
            return

            self.logger.info('TEST-START %s' % os.path.basename(test))

        self.test_kwargs['expected'] = expected
        self.test_kwargs['oop'] = oop
        for handler in self.test_handlers:
            if handler.match(os.path.basename(test)):
                handler.add_tests_to_suite(mod_name,
                                           filepath,
                                           suite,
                                           testloader,
                                           self.marionette,
                                           self.testvars,
                                           **self.test_kwargs)
                break

        if suite.countTestCases():
            runner = self.textrunnerclass(verbosity=3,
                                          marionette=self.marionette,
                                          capabilities=self.capabilities)
            results = runner.run(suite)
            self.results.append(results)

            self.failed += len(results.failures) + len(results.errors)
            if hasattr(results, 'skipped'):
                self.todo += len(results.skipped)
            self.passed += results.passed
            for failure in results.failures + results.errors:
                self.failures.append((results.getInfo(failure), failure.output, 'TEST-UNEXPECTED-FAIL'))
            if hasattr(results, 'unexpectedSuccesses'):
                self.failed += len(results.unexpectedSuccesses)
                for failure in results.unexpectedSuccesses:
                    self.failures.append((results.getInfo(failure), 'TEST-UNEXPECTED-PASS'))
            if hasattr(results, 'expectedFailures'):
                self.passed += len(results.expectedFailures)

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
        testsuite.setAttribute('time', str(self.elapsedtime.total_seconds()))
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


from datetime import datetime
import imp
import inspect
import logging
from optparse import OptionParser
import os
import types
import unittest
import socket
import sys
import time

try:
    from manifestparser import TestManifest
    from mozhttpd import iface, MozHttpd
except ImportError:
    print "manifestparser or mozhttpd not found!  Please install mozbase:\n"
    print "\tgit clone git clone git://github.com/mozilla/mozbase.git"
    print "\tpython setup_development.py\n"
    import sys
    sys.exit(1)


from marionette import Marionette
from marionette_test import MarionetteJSTestCase


class MarionetteTestResult(unittest._TextTestResult):

    def __init__(self, *args):
        super(MarionetteTestResult, self).__init__(*args)
        self.passed = 0

    def addSuccess(self, test):
        super(MarionetteTestResult, self).addSuccess(test)
        self.passed += 1

    def getInfo(self, test):
        if hasattr(test, 'jsFile'):
            return os.path.basename(test.jsFile)
        else:
            return '%s.py:%s.%s' % (test.__class__.__module__,
                                    test.__class__.__name__,
                                    test._testMethodName)

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
                print 'START LOG:'
                for line in testcase.loglines:
                    print ' '.join(line)
                print 'END LOG:'


class MarionetteTextTestRunner(unittest.TextTestRunner):

    resultclass = MarionetteTestResult

    def _makeResult(self):
        return self.resultclass(self.stream, self.descriptions, self.verbosity)

    def run(self, test):
        "Run the given test case or test suite."
        result = self._makeResult()
        result.failfast = self.failfast
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
        timeTaken = stopTime - startTime
        result.printErrors()
        result.printLogs(test)
        if hasattr(result, 'separator2'):
            self.stream.writeln(result.separator2)
        run = result.testsRun
        self.stream.writeln("Ran %d test%s in %.3fs" %
                            (run, run != 1 and "s" or "", timeTaken))
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


class MarionetteTestRunner(object):

    def __init__(self, address=None, emulator=False, homedir=None,
                 b2gbin=None, autolog=False, revision=None, es_server=None,
                 rest_server=None, logger=None, testgroup="marionette",
                 noWindow=False):
        self.address = address
        self.emulator = emulator
        self.homedir = homedir
        self.b2gbin = b2gbin
        self.autolog = autolog
        self.testgroup = testgroup
        self.revision = revision
        self.es_server = es_server
        self.rest_server = rest_server
        self.logger = logger
        self.noWindow = noWindow
        self.httpd = None
        self.baseurl = None
        self.marionette = None

        self.reset_test_stats()

        if self.logger is None:
            self.logger = logging.getLogger('Marionette')
            self.logger.setLevel(logging.INFO)
            self.logger.addHandler(logging.StreamHandler())

    def reset_test_stats(self):
        self.passed = 0
        self.failed = 0
        self.todo = 0
        self.failures = []

    def start_httpd(self):
        host = iface.get_lan_ip()
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(("",0))
        port = s.getsockname()[1]
        s.close()
        self.baseurl = 'http://%s:%d/' % (host, port)
        self.logger.info('running webserver on %s' % self.baseurl)
        self.httpd = MozHttpd(host=host,
                              port=port,
                              docroot=os.path.join(os.path.dirname(__file__), 'www'))
        self.httpd.start()

    def start_marionette(self):
        assert(self.baseurl is not None)
        if self.address:
            host, port = self.address.split(':')
            if self.emulator:
                self.marionette = Marionette(host=host, port=int(port),
                                            connectToRunningEmulator=True,
                                            homedir=self.homedir,
                                            baseurl=self.baseurl)
            if self.b2gbin:
                self.marionette = Marionette(host=host, port=int(port), b2gbin=self.b2gbin, baseurl=self.baseurl)
            else:
                self.marionette = Marionette(host=host, port=int(port), baseurl=self.baseurl)
        elif self.emulator:
            self.marionette = Marionette(emulator=True,
                                         homedir=self.homedir,
                                         baseurl=self.baseurl,
                                         noWindow=self.noWindow)
        else:
            raise Exception("must specify address or emulator")

    def post_to_autolog(self, elapsedtime):
        self.logger.info('posting results to autolog')

        # This is all autolog stuff.
        # See: https://wiki.mozilla.org/Auto-tools/Projects/Autolog
        from mozautolog import RESTfulAutologTestGroup
        testgroup = RESTfulAutologTestGroup(
            testgroup = self.testgroup,
            os = 'android',
            platform = 'emulator',
            harness = 'marionette',
            server = self.es_server,
            restserver = self.rest_server,
            machine = socket.gethostname())

        testgroup.set_primary_product(
            tree = 'b2g',
            buildtype = 'opt',
            revision = self.revision)

        testgroup.add_test_suite(
            testsuite = 'b2g emulator testsuite',
            elapsedtime = elapsedtime.seconds,
            cmdline = '',
            passed = self.passed,
            failed = self.failed,
            todo = self.todo)

        # Add in the test failures.
        for f in self.failures:
            testgroup.add_test_failure(test=f[0], text=f[1], status=f[2])

        testgroup.submit()

    def run_tests(self, tests, testtype=None):
        self.reset_test_stats()
        starttime = datetime.utcnow()
        for test in tests:
            self.run_test(test, testtype)
        self.logger.info('\nSUMMARY\n-------')
        self.logger.info('passed: %d' % self.passed)
        self.logger.info('failed: %d' % self.failed)
        self.logger.info('todo: %d' % self.todo)
        elapsedtime = datetime.utcnow() - starttime
        if self.autolog:
            self.post_to_autolog(elapsedtime)
        if self.marionette.emulator:
            self.marionette.emulator.close()
            self.marionette.emulator = None
        self.marionette = None

    def run_test(self, test, testtype):
        if not self.httpd:
            self.start_httpd()
        if not self.marionette:
            self.start_marionette()

        if not os.path.isabs(test):
            filepath = os.path.join(os.path.dirname(__file__), test)
        else:
            filepath = test

        if os.path.isdir(filepath):
            for root, dirs, files in os.walk(filepath):
                for filename in files:
                    if ((filename.startswith('test_') or filename.startswith('browser_')) and 
                        (filename.endswith('.py') or filename.endswith('.js'))):
                        filepath = os.path.join(root, filename)
                        self.run_test(filepath, testtype)
            return

        mod_name,file_ext = os.path.splitext(os.path.split(filepath)[-1])

        testloader = unittest.TestLoader()
        suite = unittest.TestSuite()

        if file_ext == '.ini':
            if testtype is not None:
                testargs = {}
                testtypes = testtype.replace('+', ' +').replace('-', ' -').split()
                for atype in testtypes:
                    if atype.startswith('+'):
                        testargs.update({ atype[1:]: 'true' })
                    elif atype.startswith('-'):
                        testargs.update({ atype[1:]: 'false' })
                    else:
                        testargs.update({ atype: 'true' })
            manifest = TestManifest()
            manifest.read(filepath)

            if testtype is None:
                manifest_tests = manifest.get()
            else:
                manifest_tests = manifest.get(**testargs)

            for i in manifest_tests:
                self.run_test(i["path"], testtype)
            return

        self.logger.info('TEST-START %s' % os.path.basename(test))

        if file_ext == '.py':
            test_mod = imp.load_source(mod_name, filepath)

            for name in dir(test_mod):
                obj = getattr(test_mod, name)
                if (isinstance(obj, (type, types.ClassType)) and
                    issubclass(obj, unittest.TestCase)):
                    testnames = testloader.getTestCaseNames(obj)
                    for testname in testnames:
                        suite.addTest(obj(self.marionette, methodName=testname))

        elif file_ext == '.js':
            suite.addTest(MarionetteJSTestCase(self.marionette, jsFile=filepath))

        if suite.countTestCases():
            results = MarionetteTextTestRunner(verbosity=3).run(suite)
            self.failed += len(results.failures) + len(results.errors)
            self.todo = 0
            if hasattr(results, 'skipped'):
                self.todo += len(results.skipped) + len(results.expectedFailures)
            self.passed += results.passed
            for failure in results.failures + results.errors:
                self.failures.append((results.getInfo(failure[0]), failure[1], 'TEST-UNEXPECTED-FAIL'))
            if hasattr(results, 'unexpectedSuccess'):
                self.failed += len(results.unexpectedSuccesses)
                for failure in results.unexpectedSuccesses:
                    self.failures.append((results.getInfo(failure[0]), failure[1], 'TEST-UNEXPECTED-PASS'))

    def cleanup(self):
        if self.httpd:
            self.httpd.stop()

    __del__ = cleanup


if __name__ == "__main__":
    parser = OptionParser(usage='%prog [options] test_file_or_dir <test_file_or_dir> ...')
    parser.add_option("--autolog",
                      action = "store_true", dest = "autolog",
                      default = False,
                      help = "send test results to autolog")
    parser.add_option("--revision",
                      action = "store", dest = "revision",
                      help = "git revision for autolog submissions")
    parser.add_option("--testgroup",
                      action = "store", dest = "testgroup",
                      help = "testgroup names for autolog submissions")
    parser.add_option("--emulator",
                      action = "store_true", dest = "emulator",
                      default = False,
                      help = "launch a B2G emulator on which to run tests")
    parser.add_option("--no-window",
                      action = "store_true", dest = "noWindow",
                      default = False,
                      help = "when Marionette launches an emulator, start it "
                      "with the -no-window argument")
    parser.add_option('--address', dest='address', action='store',
                      help='host:port of running Gecko instance to connect to')
    parser.add_option('--type', dest='type', action='store',
                      default='browser+b2g',
                      help = "The type of test to run, can be a combination "
                      "of values defined in unit-tests.ini; individual values "
                      "are combined with '+' or '-' chars.  Ex:  'browser+b2g' "
                      "means the set of tests which are compatible with both "
                      "browser and b2g; 'b2g-qemu' means the set of tests "
                      "which are compatible with b2g but do not require an "
                      "emulator.  This argument is only used when loading "
                      "tests from .ini files.")
    parser.add_option('--homedir', dest='homedir', action='store',
                      help='home directory of emulator files')
    parser.add_option('--b2gbin', dest='b2gbin', action='store',
                      help='b2g executable')

    options, tests = parser.parse_args()

    if not tests:
        parser.print_usage()
        parser.exit()

    if not options.emulator and not options.address:
        parser.print_usage()
        print "must specify --emulator or --address"
        parser.exit()

    runner = MarionetteTestRunner(address=options.address,
                                  emulator=options.emulator,
                                  homedir=options.homedir,
                                  b2gbin=options.b2gbin,
                                  noWindow=options.noWindow,
                                  revision=options.revision,
                                  testgroup=options.testgroup,
                                  autolog=options.autolog)
    runner.run_tests(tests, testtype=options.type)
    if runner.failed > 0:
        sys.exit(10)




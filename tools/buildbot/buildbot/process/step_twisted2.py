#! /usr/bin/python

from buildbot.status import tests
from buildbot.process.step import SUCCESS, FAILURE, BuildStep
from buildbot.process.step_twisted import RunUnitTests

from zope.interface import implements
from twisted.python import log, failure
from twisted.spread import jelly
from twisted.pb.tokens import BananaError
from twisted.web.html import PRE
from twisted.web.error import NoResource

class Null: pass
ResultTypes = Null()
ResultTypeNames = ["SKIP",
                   "EXPECTED_FAILURE", "FAILURE", "ERROR",
                   "UNEXPECTED_SUCCESS", "SUCCESS"]
try:
    from twisted.trial import reporter # introduced in Twisted-1.0.5
    # extract the individual result types
    for name in ResultTypeNames:
        setattr(ResultTypes, name, getattr(reporter, name))
except ImportError:
    from twisted.trial import unittest # Twisted-1.0.4 has them here
    for name in ResultTypeNames:
        setattr(ResultTypes, name, getattr(unittest, name))

log._keepErrors = 0
from twisted.trial import remote # for trial/jelly parsing

import StringIO

class OneJellyTest(tests.OneTest):
    def html(self, request):
        tpl = "<HTML><BODY>\n\n%s\n\n</body></html>\n"
        pptpl = "<HTML><BODY>\n\n<pre>%s</pre>\n\n</body></html>\n"
        t = request.postpath[0] # one of 'short', 'long' #, or 'html'
        if isinstance(self.results, failure.Failure):
            # it would be nice to remove unittest functions from the
            # traceback like unittest.format_exception() does.
            if t == 'short':
                s = StringIO.StringIO()
                self.results.printTraceback(s)
                return pptpl % PRE(s.getvalue())
            elif t == 'long':
                s = StringIO.StringIO()
                self.results.printDetailedTraceback(s)
                return pptpl % PRE(s.getvalue())
            #elif t == 'html':
            #    return tpl % formatFailure(self.results)
            # ACK! source lines aren't stored in the Failure, rather,
            # formatFailure pulls them (by filename) from the local
            # disk. Feh. Even printTraceback() won't work. Double feh.
            return NoResource("No such mode '%s'" % t)
        if self.results == None:
            return tpl % "No results to show: test probably passed."
        # maybe results are plain text?
        return pptpl % PRE(self.results)

class TwistedJellyTestResults(tests.TestResults):
    oneTestClass = OneJellyTest
    def describeOneTest(self, testname):
        return "%s: %s\n" % (testname, self.tests[testname][0])

class RunUnitTestsJelly(RunUnitTests):
    """I run the unit tests with the --jelly option, which generates
    machine-parseable results as the tests are run.
    """
    trialMode = "--jelly"
    implements(remote.IRemoteReporter)

    ourtypes = { ResultTypes.SKIP: tests.SKIP,
                 ResultTypes.EXPECTED_FAILURE: tests.EXPECTED_FAILURE,
                 ResultTypes.FAILURE: tests.FAILURE,
                 ResultTypes.ERROR: tests.ERROR,
                 ResultTypes.UNEXPECTED_SUCCESS: tests.UNEXPECTED_SUCCESS,
                 ResultTypes.SUCCESS: tests.SUCCESS,
                 }

    def __getstate__(self):
        #d = RunUnitTests.__getstate__(self)
        d = self.__dict__.copy()
        # Banana subclasses are Ephemeral
        if d.has_key("decoder"):
            del d['decoder']
        return d
    def start(self):
        self.decoder = remote.DecodeReport(self)
        # don't accept anything unpleasant from the (untrusted) build slave
        # The jellied stream may have Failures, but everything inside should
        # be a string
        security = jelly.SecurityOptions()
        security.allowBasicTypes()
        security.allowInstancesOf(failure.Failure)
        self.decoder.taster = security
        self.results = TwistedJellyTestResults()
        RunUnitTests.start(self)

    def logProgress(self, progress):
        # XXX: track number of tests
        BuildStep.logProgress(self, progress)

    def addStdout(self, data):
        if not self.decoder:
            return
        try:
            self.decoder.dataReceived(data)
        except BananaError:
            self.decoder = None
            log.msg("trial --jelly output unparseable, traceback follows")
            log.deferr()

    def remote_start(self, expectedTests, times=None):
        print "remote_start", expectedTests
    def remote_reportImportError(self, name, aFailure, times=None):
        pass
    def remote_reportStart(self, testClass, method, times=None):
        print "reportStart", testClass, method

    def remote_reportResults(self, testClass, method, resultType, results,
                             times=None):
        print "reportResults", testClass, method, resultType
        which = testClass + "." + method
        self.results.addTest(which,
                             self.ourtypes.get(resultType, tests.UNKNOWN),
                             results)

    def finished(self, rc):
        # give self.results to our Build object
        self.build.testsFinished(self.results)
        total = self.results.countTests()
        count = self.results.countFailures()
        result = SUCCESS
        if total == None:
            result = (FAILURE, ['tests%s' % self.rtext(' (%s)')])
        if count:
            result = (FAILURE, ["%d tes%s%s" % (count,
                                                (count == 1 and 't' or 'ts'),
                                                self.rtext(' (%s)'))])
        return self.stepComplete(result)
    def finishStatus(self, result):
        total = self.results.countTests()
        count = self.results.countFailures()
        color = "green"
        text = []
        if count == 0:
            text.extend(["%d %s" % \
                         (total,
                          total == 1 and "test" or "tests"),
                         "passed"])
        else:
            text.append("tests")
            text.append("%d %s" % \
                        (count,
                         count == 1 and "failure" or "failures"))
            color = "red"
        self.updateCurrentActivity(color=color, text=text)
        self.addFileToCurrentActivity("tests", self.results)
        #self.finishStatusSummary()
        self.finishCurrentActivity()
            

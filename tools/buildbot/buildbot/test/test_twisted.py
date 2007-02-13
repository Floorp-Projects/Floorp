# -*- test-case-name: buildbot.test.test_twisted -*-

from twisted.trial import unittest

from buildbot import interfaces
from buildbot.steps.python_twisted import countFailedTests
from buildbot.steps.python_twisted import Trial, TrialTestCaseCounter
from buildbot.status import builder

noisy = 0
if noisy:
    from twisted.python.log import startLogging
    import sys
    startLogging(sys.stdout)

out1 = """
-------------------------------------------------------------------------------
Ran 13 tests in 1.047s

OK
"""

out2 = """
-------------------------------------------------------------------------------
Ran 12 tests in 1.040s

FAILED (failures=1)
"""

out3 = """
 NotImplementedError
-------------------------------------------------------------------------------
Ran 13 tests in 1.042s

FAILED (failures=1, errors=1)
"""

out4 = """
unparseable
"""

out5 = """
   File "/usr/home/warner/stuff/python/twisted/Twisted-CVS/twisted/test/test_defer.py", line 79, in testTwoCallbacks
    self.fail("just because")
   File "/usr/home/warner/stuff/python/twisted/Twisted-CVS/twisted/trial/unittest.py", line 21, in fail
    raise AssertionError, message
 AssertionError: just because
unparseable
"""

out6 = """
===============================================================================
SKIPPED: testProtocolLocalhost (twisted.flow.test.test_flow.FlowTest)
-------------------------------------------------------------------------------
XXX freezes, fixme
===============================================================================
SKIPPED: testIPv6 (twisted.names.test.test_names.HostsTestCase)
-------------------------------------------------------------------------------
IPv6 support is not in our hosts resolver yet
===============================================================================
EXPECTED FAILURE: testSlots (twisted.test.test_rebuild.NewStyleTestCase)
-------------------------------------------------------------------------------
Traceback (most recent call last):
  File "/Users/buildbot/Buildbot/twisted/OSX-full2.3/Twisted/twisted/trial/unittest.py", line 240, in _runPhase
    stage(*args, **kwargs)
  File "/Users/buildbot/Buildbot/twisted/OSX-full2.3/Twisted/twisted/trial/unittest.py", line 262, in _main
    self.runner(self.method)
  File "/Users/buildbot/Buildbot/twisted/OSX-full2.3/Twisted/twisted/trial/runner.py", line 95, in runTest
    method()
  File "/Users/buildbot/Buildbot/twisted/OSX-full2.3/Twisted/twisted/test/test_rebuild.py", line 130, in testSlots
    rebuild.updateInstance(self.m.SlottedClass())
  File "/Users/buildbot/Buildbot/twisted/OSX-full2.3/Twisted/twisted/python/rebuild.py", line 114, in updateInstance
    self.__class__ = latestClass(self.__class__)
TypeError: __class__ assignment: 'SlottedClass' object layout differs from 'SlottedClass'
===============================================================================
FAILURE: testBatchFile (twisted.conch.test.test_sftp.TestOurServerBatchFile)
-------------------------------------------------------------------------------
Traceback (most recent call last):
  File "/Users/buildbot/Buildbot/twisted/OSX-full2.3/Twisted/twisted/trial/unittest.py", line 240, in _runPhase
    stage(*args, **kwargs)
  File "/Users/buildbot/Buildbot/twisted/OSX-full2.3/Twisted/twisted/trial/unittest.py", line 262, in _main
    self.runner(self.method)
  File "/Users/buildbot/Buildbot/twisted/OSX-full2.3/Twisted/twisted/trial/runner.py", line 95, in runTest
    method()
  File "/Users/buildbot/Buildbot/twisted/OSX-full2.3/Twisted/twisted/conch/test/test_sftp.py", line 450, in testBatchFile
    self.failUnlessEqual(res[1:-2], ['testDirectory', 'testRemoveFile', 'testRenameFile', 'testfile1'])
  File "/Users/buildbot/Buildbot/twisted/OSX-full2.3/Twisted/twisted/trial/unittest.py", line 115, in failUnlessEqual
    raise FailTest, (msg or '%r != %r' % (first, second))
FailTest: [] != ['testDirectory', 'testRemoveFile', 'testRenameFile', 'testfile1']
-------------------------------------------------------------------------------
Ran 1454 tests in 911.579s

FAILED (failures=2, skips=49, expectedFailures=9)
Exception exceptions.AttributeError: "'NoneType' object has no attribute 'StringIO'" in <bound method RemoteReference.__del__ of <twisted.spread.pb.RemoteReference instance at 0x27036c0>> ignored
"""

class MyTrial(Trial):
    def addTestResult(self, testname, results, text, logs):
        self.results.append((testname, results, text, logs))
    def addCompleteLog(self, name, log):
        pass

class MyLogFile:
    def __init__(self, text):
        self.text = text
    def getText(self):
        return self.text


class Count(unittest.TestCase):

    def count(self, total, failures=0, errors=0,
              expectedFailures=0, unexpectedSuccesses=0, skips=0):
        d = {
            'total': total,
            'failures': failures,
            'errors': errors,
            'expectedFailures': expectedFailures,
            'unexpectedSuccesses': unexpectedSuccesses,
            'skips': skips,
            }
        return d

    def testCountFailedTests(self):
        count = countFailedTests(out1)
        self.assertEquals(count, self.count(total=13))
        count = countFailedTests(out2)
        self.assertEquals(count, self.count(total=12, failures=1))
        count = countFailedTests(out3)
        self.assertEquals(count, self.count(total=13, failures=1, errors=1))
        count = countFailedTests(out4)
        self.assertEquals(count, self.count(total=None))
        count = countFailedTests(out5)
        self.assertEquals(count, self.count(total=None))

class Counter(unittest.TestCase):

    def setProgress(self, metric, value):
        self.progress = (metric, value)

    def testCounter(self):
        self.progress = (None,None)
        c = TrialTestCaseCounter()
        c.setStep(self)
        STDOUT = interfaces.LOG_CHANNEL_STDOUT
        def add(text):
            c.logChunk(None, None, None, STDOUT, text)
        add("\n\n")
        self.failUnlessEqual(self.progress, (None,None))
        add("bogus line\n")
        self.failUnlessEqual(self.progress, (None,None))
        add("buildbot.test.test_config.ConfigTest.testBots ... [OK]\n")
        self.failUnlessEqual(self.progress, ("tests", 1))
        add("buildbot.test.test_config.ConfigTest.tes")
        self.failUnlessEqual(self.progress, ("tests", 1))
        add("tBuilders ... [OK]\n")
        self.failUnlessEqual(self.progress, ("tests", 2))
        # confirm alternative delimiters work too.. ptys seem to emit
        # something different
        add("buildbot.test.test_config.ConfigTest.testIRC ... [OK]\r\n")
        self.failUnlessEqual(self.progress, ("tests", 3))
        add("===============================================================================\n")
        self.failUnlessEqual(self.progress, ("tests", 3))
        add("buildbot.test.test_config.IOnlyLookLikeA.testLine ... [OK]\n")
        self.failUnlessEqual(self.progress, ("tests", 3))



class Parse(unittest.TestCase):
    def failUnlessIn(self, substr, string):
        self.failUnless(string.find(substr) != -1)

    def testParse(self):
        t = MyTrial(build=None, workdir=".", testpath=None, testChanges=True)
        t.results = []
        log = MyLogFile(out6)
        t.createSummary(log)

        self.failUnlessEqual(len(t.results), 4)
        r1, r2, r3, r4 = t.results
        testname, results, text, logs = r1
        self.failUnlessEqual(testname,
                             ("twisted", "flow", "test", "test_flow",
                              "FlowTest", "testProtocolLocalhost"))
        self.failUnlessEqual(results, builder.SKIPPED)
        self.failUnlessEqual(text, ['skipped'])
        self.failUnlessIn("XXX freezes, fixme", logs)
        self.failUnless(logs.startswith("SKIPPED:"))
        self.failUnless(logs.endswith("fixme\n"))

        testname, results, text, logs = r2
        self.failUnlessEqual(testname,
                             ("twisted", "names", "test", "test_names",
                              "HostsTestCase", "testIPv6"))
        self.failUnlessEqual(results, builder.SKIPPED)
        self.failUnlessEqual(text, ['skipped'])
        self.failUnless(logs.startswith("SKIPPED: testIPv6"))
        self.failUnless(logs.endswith("IPv6 support is not in our hosts resolver yet\n"))

        testname, results, text, logs = r3
        self.failUnlessEqual(testname,
                             ("twisted", "test", "test_rebuild",
                              "NewStyleTestCase", "testSlots"))
        self.failUnlessEqual(results, builder.SUCCESS)
        self.failUnlessEqual(text, ['expected', 'failure'])
        self.failUnless(logs.startswith("EXPECTED FAILURE: "))
        self.failUnlessIn("\nTraceback ", logs)
        self.failUnless(logs.endswith("layout differs from 'SlottedClass'\n"))

        testname, results, text, logs = r4
        self.failUnlessEqual(testname,
                             ("twisted", "conch", "test", "test_sftp",
                              "TestOurServerBatchFile", "testBatchFile"))
        self.failUnlessEqual(results, builder.FAILURE)
        self.failUnlessEqual(text, ['failure'])
        self.failUnless(logs.startswith("FAILURE: "))
        self.failUnlessIn("Traceback ", logs)
        self.failUnless(logs.endswith("'testRenameFile', 'testfile1']\n"))


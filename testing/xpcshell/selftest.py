#!/usr/bin/env python
#
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/
#

from __future__ import with_statement
import sys, os, unittest, tempfile, shutil
import mozinfo

from StringIO import StringIO
from xml.etree.ElementTree import ElementTree

from mozbuild.base import MozbuildObject
build_obj = MozbuildObject.from_environment()

from runxpcshelltests import XPCShellTests

mozinfo.find_and_update_from_json()

objdir = build_obj.topobjdir.encode("utf-8")
xpcshellBin = os.path.join(objdir, "dist", "bin", "xpcshell")
if sys.platform == "win32":
    xpcshellBin += ".exe"

SIMPLE_PASSING_TEST = "function run_test() { do_check_true(true); }"
SIMPLE_FAILING_TEST = "function run_test() { do_check_true(false); }"

ADD_TEST_SIMPLE = '''
function run_test() { run_next_test(); }

add_test(function test_simple() {
  do_check_true(true);
  run_next_test();
});
'''

ADD_TEST_FAILING = '''
function run_test() { run_next_test(); }

add_test(function test_failing() {
  do_check_true(false);
  run_next_test();
});
'''

CHILD_TEST_PASSING = '''
function run_test () { run_next_test(); }

add_test(function test_child_simple () {
  run_test_in_child("test_pass.js");
  run_next_test();
});
'''

CHILD_TEST_FAILING = '''
function run_test () { run_next_test(); }

add_test(function test_child_simple () {
  run_test_in_child("test_fail.js");
  run_next_test();
});
'''

CHILD_TEST_HANG = '''
function run_test () { run_next_test(); }

add_test(function test_child_simple () {
  do_test_pending("hang test");
  do_load_child_test_harness();
  sendCommand("_log('child_test_start', {_message: 'CHILD-TEST-STARTED'}); " +
              + "const _TEST_FILE=['test_pass.js']; _execute_test(); ",
              do_test_finished);
  run_next_test();
});
'''

ADD_TASK_SINGLE = '''
Components.utils.import("resource://gre/modules/Promise.jsm");

function run_test() { run_next_test(); }

add_task(function test_task() {
  yield Promise.resolve(true);
  yield Promise.resolve(false);
});
'''

ADD_TASK_MULTIPLE = '''
Components.utils.import("resource://gre/modules/Promise.jsm");

function run_test() { run_next_test(); }

add_task(function test_task() {
  yield Promise.resolve(true);
});

add_task(function test_2() {
  yield Promise.resolve(true);
});
'''

ADD_TASK_REJECTED = '''
Components.utils.import("resource://gre/modules/Promise.jsm");

function run_test() { run_next_test(); }

add_task(function test_failing() {
  yield Promise.reject(new Error("I fail."));
});
'''

ADD_TASK_FAILURE_INSIDE = '''
Components.utils.import("resource://gre/modules/Promise.jsm");

function run_test() { run_next_test(); }

add_task(function test() {
  let result = yield Promise.resolve(false);

  do_check_true(result);
});
'''

ADD_TASK_RUN_NEXT_TEST = '''
function run_test() { run_next_test(); }

add_task(function () {
  Assert.ok(true);

  run_next_test();
});
'''

ADD_TASK_STACK_TRACE = '''
Components.utils.import("resource://gre/modules/Promise.jsm", this);

function run_test() { run_next_test(); }

add_task(function* this_test_will_fail() {
  for (let i = 0; i < 10; ++i) {
    yield Promise.resolve();
  }
  Assert.ok(false);
});
'''

ADD_TASK_STACK_TRACE_WITHOUT_STAR = '''
Components.utils.import("resource://gre/modules/Promise.jsm", this);

function run_test() { run_next_test(); }

add_task(function this_test_will_fail() {
  for (let i = 0; i < 10; ++i) {
    yield Promise.resolve();
  }
  Assert.ok(false);
});
'''

ADD_TEST_THROW_STRING = '''
function run_test() {do_throw("Passing a string to do_throw")};
'''

ADD_TEST_THROW_OBJECT = '''
let error = {
  message: "Error object",
  fileName: "failure.js",
  stack: "ERROR STACK",
  toString: function() {return this.message;}
};
function run_test() {do_throw(error)};
'''

ADD_TEST_REPORT_OBJECT = '''
let error = {
  message: "Error object",
  fileName: "failure.js",
  stack: "ERROR STACK",
  toString: function() {return this.message;}
};
function run_test() {do_report_unexpected_exception(error)};
'''

# A test for genuine JS-generated Error objects
ADD_TEST_REPORT_REF_ERROR = '''
function run_test() {
  let obj = {blah: 0};
  try {
    obj.noSuchFunction();
  }
  catch (error) {
    do_report_unexpected_exception(error);
  }
};
'''

# A test for failure to load a test due to a syntax error
LOAD_ERROR_SYNTAX_ERROR = '''
function run_test(
'''

# A test for failure to load a test due to an error other than a syntax error
LOAD_ERROR_OTHER_ERROR = '''
function run_test() {
    yield "foo";
    return "foo"; // can't use return in a generator!
};
'''

# A test for asynchronous cleanup functions
ASYNC_CLEANUP = '''
function run_test() {
  Components.utils.import("resource://gre/modules/Promise.jsm", this);

  // The list of checkpoints in the order we encounter them.
  let checkpoints = [];

  // Cleanup tasks, in reverse order
  do_register_cleanup(function cleanup_checkout() {
    do_check_eq(checkpoints.join(""), "1234");
    do_print("At this stage, the test has succeeded");
    do_throw("Throwing an error to force displaying the log");
  });

  do_register_cleanup(function sync_cleanup_2() {
    checkpoints.push(4);
  });

  do_register_cleanup(function async_cleanup_2() {
    let deferred = Promise.defer();
    do_execute_soon(deferred.resolve);
    return deferred.promise.then(function() {
      checkpoints.push(3);
    });
  });

  do_register_cleanup(function sync_cleanup() {
    checkpoints.push(2);
  });

  do_register_cleanup(function async_cleanup() {
    let deferred = Promise.defer();
    do_execute_soon(deferred.resolve);
    return deferred.promise.then(function() {
      checkpoints.push(1);
    });
  });

}
'''


class XPCShellTestsTests(unittest.TestCase):
    """
    Yes, these are unit tests for a unit test harness.
    """
    def setUp(self):
        self.log = StringIO()
        self.tempdir = tempfile.mkdtemp()
        self.x = XPCShellTests(log=self.log)

    def tearDown(self):
        shutil.rmtree(self.tempdir)

    def writeFile(self, name, contents):
        """
        Write |contents| to a file named |name| in the temp directory,
        and return the full path to the file.
        """
        fullpath = os.path.join(self.tempdir, name)
        with open(fullpath, "w") as f:
            f.write(contents)
        return fullpath

    def writeManifest(self, tests):
        """
        Write an xpcshell.ini in the temp directory and set
        self.manifest to its pathname. |tests| is a list containing
        either strings (for test names), or tuples with a test name
        as the first element and manifest conditions as the following
        elements.
        """
        testlines = []
        for t in tests:
            testlines.append("[%s]" % (t if isinstance(t, basestring)
                                       else t[0]))
            if isinstance(t, tuple):
                testlines.extend(t[1:])
        self.manifest = self.writeFile("xpcshell.ini", """
[DEFAULT]
head =
tail =

""" + "\n".join(testlines))

    def assertTestResult(self, expected, shuffle=False, xunitFilename=None, verbose=False):
        """
        Assert that self.x.runTests with manifest=self.manifest
        returns |expected|.
        """
        self.assertEquals(expected,
                          self.x.runTests(xpcshellBin,
                                          manifest=self.manifest,
                                          mozInfo=mozinfo.info,
                                          shuffle=shuffle,
                                          testsRootDir=self.tempdir,
                                          verbose=verbose,
                                          xunitFilename=xunitFilename,
                                          sequential=True),
                          msg="""Tests should have %s, log:
========
%s
========
""" % ("passed" if expected else "failed", self.log.getvalue()))

    def _assertLog(self, s, expected):
        l = self.log.getvalue()
        self.assertEqual(expected, s in l,
                         msg="""Value %s %s in log:
========
%s
========""" % (s, "expected" if expected else "not expected", l))

    def assertInLog(self, s):
        """
        Assert that the string |s| is contained in self.log.
        """
        self._assertLog(s, True)

    def assertNotInLog(self, s):
        """
        Assert that the string |s| is not contained in self.log.
        """
        self._assertLog(s, False)

    def testPass(self):
        """
        Check that a simple test without any manifest conditions passes.
        """
        self.writeFile("test_basic.js", SIMPLE_PASSING_TEST)
        self.writeManifest(["test_basic.js"])

        self.assertTestResult(True)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(1, self.x.passCount)
        self.assertEquals(0, self.x.failCount)
        self.assertEquals(0, self.x.todoCount)
        self.assertInLog("TEST-PASS")
        self.assertNotInLog("TEST-UNEXPECTED-FAIL")

    def testFail(self):
        """
        Check that a simple failing test without any manifest conditions fails.
        """
        self.writeFile("test_basic.js", SIMPLE_FAILING_TEST)
        self.writeManifest(["test_basic.js"])

        self.assertTestResult(False)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(0, self.x.passCount)
        self.assertEquals(1, self.x.failCount)
        self.assertEquals(0, self.x.todoCount)
        self.assertInLog("TEST-UNEXPECTED-FAIL")
        self.assertNotInLog("TEST-PASS")

    @unittest.skipIf(build_obj.defines.get('MOZ_B2G'),
                     'selftests with child processes fail on b2g desktop builds')
    def testChildPass(self):
        """
        Check that a simple test running in a child process passes.
        """
        self.writeFile("test_pass.js", SIMPLE_PASSING_TEST)
        self.writeFile("test_child_pass.js", CHILD_TEST_PASSING)
        self.writeManifest(["test_child_pass.js"])

        self.assertTestResult(True, verbose=True)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(1, self.x.passCount)
        self.assertEquals(0, self.x.failCount)
        self.assertEquals(0, self.x.todoCount)
        self.assertInLog("TEST-PASS")
        self.assertInLog("CHILD-TEST-STARTED")
        self.assertInLog("CHILD-TEST-COMPLETED")
        self.assertNotInLog("TEST-UNEXPECTED-FAIL")


    @unittest.skipIf(build_obj.defines.get('MOZ_B2G'),
                     'selftests with child processes fail on b2g desktop builds')
    def testChildFail(self):
        """
        Check that a simple failing test running in a child process fails.
        """
        self.writeFile("test_fail.js", SIMPLE_FAILING_TEST)
        self.writeFile("test_child_fail.js", CHILD_TEST_FAILING)
        self.writeManifest(["test_child_fail.js"])

        self.assertTestResult(False)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(0, self.x.passCount)
        self.assertEquals(1, self.x.failCount)
        self.assertEquals(0, self.x.todoCount)
        self.assertInLog("TEST-UNEXPECTED-FAIL")
        self.assertInLog("CHILD-TEST-STARTED")
        self.assertInLog("CHILD-TEST-COMPLETED")
        self.assertNotInLog("TEST-PASS")

    @unittest.skipIf(build_obj.defines.get('MOZ_B2G'),
                     'selftests with child processes fail on b2g desktop builds')
    def testChildHang(self):
        """
        Check that incomplete output from a child process results in a
        test failure.
        """
        self.writeFile("test_pass.js", SIMPLE_PASSING_TEST)
        self.writeFile("test_child_hang.js", CHILD_TEST_HANG)
        self.writeManifest(["test_child_hang.js"])

        self.assertTestResult(False)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(0, self.x.passCount)
        self.assertEquals(1, self.x.failCount)
        self.assertEquals(0, self.x.todoCount)
        self.assertInLog("TEST-UNEXPECTED-FAIL")
        self.assertInLog("CHILD-TEST-STARTED")
        self.assertNotInLog("CHILD-TEST-COMPLETED")
        self.assertNotInLog("TEST-PASS")

    def testSyntaxError(self):
        """
        Check that running a test file containing a syntax error produces
        a test failure and expected output.
        """
        self.writeFile("test_syntax_error.js", '"')
        self.writeManifest(["test_syntax_error.js"])

        self.assertTestResult(False, verbose=True)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(0, self.x.passCount)
        self.assertEquals(1, self.x.failCount)
        self.assertEquals(0, self.x.todoCount)
        self.assertInLog("TEST-UNEXPECTED-FAIL")
        self.assertNotInLog("TEST-PASS")

    def testPassFail(self):
        """
        Check that running more than one test works.
        """
        self.writeFile("test_pass.js", SIMPLE_PASSING_TEST)
        self.writeFile("test_fail.js", SIMPLE_FAILING_TEST)
        self.writeManifest(["test_pass.js", "test_fail.js"])

        self.assertTestResult(False)
        self.assertEquals(2, self.x.testCount)
        self.assertEquals(1, self.x.passCount)
        self.assertEquals(1, self.x.failCount)
        self.assertEquals(0, self.x.todoCount)
        self.assertInLog("TEST-PASS")
        self.assertInLog("TEST-UNEXPECTED-FAIL")

    def testSkip(self):
        """
        Check that a simple failing test skipped in the manifest does
        not cause failure.
        """
        self.writeFile("test_basic.js", SIMPLE_FAILING_TEST)
        self.writeManifest([("test_basic.js", "skip-if = true")])
        self.assertTestResult(True)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(0, self.x.passCount)
        self.assertEquals(0, self.x.failCount)
        self.assertEquals(0, self.x.todoCount)
        self.assertNotInLog("TEST-UNEXPECTED-FAIL")
        self.assertNotInLog("TEST-PASS")

    def testKnownFail(self):
        """
        Check that a simple failing test marked as known-fail in the manifest
        does not cause failure.
        """
        self.writeFile("test_basic.js", SIMPLE_FAILING_TEST)
        self.writeManifest([("test_basic.js", "fail-if = true")])
        self.assertTestResult(True)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(0, self.x.passCount)
        self.assertEquals(0, self.x.failCount)
        self.assertEquals(1, self.x.todoCount)
        self.assertInLog("TEST-KNOWN-FAIL")
        # This should be suppressed because the harness doesn't include
        # the full log from the xpcshell run when things pass.
        self.assertNotInLog("TEST-UNEXPECTED-FAIL")
        self.assertNotInLog("TEST-PASS")

    def testUnexpectedPass(self):
        """
        Check that a simple failing test marked as known-fail in the manifest
        that passes causes an unexpected pass.
        """
        self.writeFile("test_basic.js", SIMPLE_PASSING_TEST)
        self.writeManifest([("test_basic.js", "fail-if = true")])
        self.assertTestResult(False)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(0, self.x.passCount)
        self.assertEquals(1, self.x.failCount)
        self.assertEquals(0, self.x.todoCount)
        # From the outer (Python) harness
        self.assertInLog("TEST-UNEXPECTED-PASS")
        self.assertNotInLog("TEST-KNOWN-FAIL")
        # From the inner (JS) harness
        self.assertInLog("TEST-PASS")

    def testReturnNonzero(self):
        """
        Check that a test where xpcshell returns nonzero fails.
        """
        self.writeFile("test_error.js", "throw 'foo'")
        self.writeManifest(["test_error.js"])

        self.assertTestResult(False)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(0, self.x.passCount)
        self.assertEquals(1, self.x.failCount)
        self.assertEquals(0, self.x.todoCount)
        self.assertInLog("TEST-UNEXPECTED-FAIL")
        self.assertNotInLog("TEST-PASS")

    def testAddTestSimple(self):
        """
        Ensure simple add_test() works.
        """
        self.writeFile("test_add_test_simple.js", ADD_TEST_SIMPLE)
        self.writeManifest(["test_add_test_simple.js"])

        self.assertTestResult(True)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(1, self.x.passCount)
        self.assertEquals(0, self.x.failCount)

    def testAddTestFailing(self):
        """
        Ensure add_test() with a failing test is reported.
        """
        self.writeFile("test_add_test_failing.js", ADD_TEST_FAILING)
        self.writeManifest(["test_add_test_failing.js"])

        self.assertTestResult(False)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(0, self.x.passCount)
        self.assertEquals(1, self.x.failCount)

    def testAddTaskTestSingle(self):
        """
        Ensure add_test_task() with a single passing test works.
        """
        self.writeFile("test_add_task_simple.js", ADD_TASK_SINGLE)
        self.writeManifest(["test_add_task_simple.js"])

        self.assertTestResult(True)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(1, self.x.passCount)
        self.assertEquals(0, self.x.failCount)

    def testAddTaskTestMultiple(self):
        """
        Ensure multiple calls to add_test_task() work as expected.
        """
        self.writeFile("test_add_task_multiple.js",
            ADD_TASK_MULTIPLE)
        self.writeManifest(["test_add_task_multiple.js"])

        self.assertTestResult(True)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(1, self.x.passCount)
        self.assertEquals(0, self.x.failCount)

    def testAddTaskTestRejected(self):
        """
        Ensure rejected task reports as failure.
        """
        self.writeFile("test_add_task_rejected.js",
            ADD_TASK_REJECTED)
        self.writeManifest(["test_add_task_rejected.js"])

        self.assertTestResult(False)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(0, self.x.passCount)
        self.assertEquals(1, self.x.failCount)

    def testAddTaskTestFailureInside(self):
        """
        Ensure tests inside task are reported as failures.
        """
        self.writeFile("test_add_task_failure_inside.js",
            ADD_TASK_FAILURE_INSIDE)
        self.writeManifest(["test_add_task_failure_inside.js"])

        self.assertTestResult(False)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(0, self.x.passCount)
        self.assertEquals(1, self.x.failCount)

    def testAddTaskRunNextTest(self):
        """
        Calling run_next_test() from inside add_task() results in failure.
        """
        self.writeFile("test_add_task_run_next_test.js",
            ADD_TASK_RUN_NEXT_TEST)
        self.writeManifest(["test_add_task_run_next_test.js"])

        self.assertTestResult(False)
        self.assertEquals(1, self.x.testCount)
        self.assertEquals(0, self.x.passCount)
        self.assertEquals(1, self.x.failCount)

    def testAddTaskStackTrace(self):
        """
        Ensuring that calling Assert.ok(false) from inside add_task()
        results in a human-readable stack trace.
        """
        self.writeFile("test_add_task_stack_trace.js",
            ADD_TASK_STACK_TRACE)
        self.writeManifest(["test_add_task_stack_trace.js"])

        self.assertTestResult(False)
        self.assertInLog("this_test_will_fail")
        self.assertInLog("run_next_test")
        self.assertInLog("run_test")
        self.assertNotInLog("Task.jsm")

    def testAddTaskStackTraceWithoutStar(self):
        """
        Ensuring that calling Assert.ok(false) from inside add_task()
        results in a human-readable stack trace. This variant uses deprecated
        `function()` syntax instead of now standard `function*()`.
        """
        self.writeFile("test_add_task_stack_trace_without_star.js",
            ADD_TASK_STACK_TRACE)
        self.writeManifest(["test_add_task_stack_trace_without_star.js"])

        self.assertTestResult(False)
        self.assertInLog("this_test_will_fail")
        self.assertInLog("run_next_test")
        self.assertInLog("run_test")
        self.assertNotInLog("Task.jsm")

    def testMissingHeadFile(self):
        """
        Ensure that missing head file results in fatal error.
        """
        self.writeFile("test_basic.js", SIMPLE_PASSING_TEST)
        self.writeManifest([("test_basic.js", "head = missing.js")])

        raised = False

        try:
            # The actual return value is never checked because we raise.
            self.assertTestResult(True)
        except Exception, ex:
            raised = True
            self.assertEquals(ex.message[0:9], "head file")

        self.assertTrue(raised)

    def testMissingTailFile(self):
        """
        Ensure that missing tail file results in fatal error.
        """
        self.writeFile("test_basic.js", SIMPLE_PASSING_TEST)
        self.writeManifest([("test_basic.js", "tail = missing.js")])

        raised = False

        try:
            self.assertTestResult(True)
        except Exception, ex:
            raised = True
            self.assertEquals(ex.message[0:9], "tail file")

        self.assertTrue(raised)

    def testRandomExecution(self):
        """
        Check that random execution doesn't break.
        """
        manifest = []
        for i in range(0, 10):
            filename = "test_pass_%d.js" % i
            self.writeFile(filename, SIMPLE_PASSING_TEST)
            manifest.append(filename)

        self.writeManifest(manifest)
        self.assertTestResult(True, shuffle=True)
        self.assertEquals(10, self.x.testCount)
        self.assertEquals(10, self.x.passCount)

    def testXunitOutput(self):
        """
        Check that Xunit XML files are written.
        """
        self.writeFile("test_00.js", SIMPLE_PASSING_TEST)
        self.writeFile("test_01.js", SIMPLE_FAILING_TEST)
        self.writeFile("test_02.js", SIMPLE_PASSING_TEST)

        manifest = [
            "test_00.js",
            "test_01.js",
            ("test_02.js", "skip-if = true")
        ]

        self.writeManifest(manifest)

        filename = os.path.join(self.tempdir, "xunit.xml")

        self.assertTestResult(False, xunitFilename=filename)

        self.assertTrue(os.path.exists(filename))
        self.assertTrue(os.path.getsize(filename) > 0)

        tree = ElementTree()
        tree.parse(filename)
        suite = tree.getroot()

        self.assertTrue(suite is not None)
        self.assertEqual(suite.get("tests"), "3")
        self.assertEqual(suite.get("failures"), "1")
        self.assertEqual(suite.get("skip"), "1")

        testcases = suite.findall("testcase")
        self.assertEqual(len(testcases), 3)

        for testcase in testcases:
            attributes = testcase.keys()
            self.assertTrue("classname" in attributes)
            self.assertTrue("name" in attributes)
            self.assertTrue("time" in attributes)

        self.assertTrue(testcases[1].find("failure") is not None)
        self.assertTrue(testcases[2].find("skipped") is not None)

    def testDoThrowString(self):
        """
        Check that do_throw produces reasonable messages when the
        input is a string instead of an object
        """
        self.writeFile("test_error.js", ADD_TEST_THROW_STRING)
        self.writeManifest(["test_error.js"])

        self.assertTestResult(False)
        self.assertInLog("TEST-UNEXPECTED-FAIL")
        self.assertInLog("Passing a string to do_throw")
        self.assertNotInLog("TEST-PASS")

    def testDoThrowForeignObject(self):
        """
        Check that do_throw produces reasonable messages when the
        input is a generic object with 'filename', 'message' and 'stack' attributes
        but 'object instanceof Error' returns false
        """
        self.writeFile("test_error.js", ADD_TEST_THROW_OBJECT)
        self.writeManifest(["test_error.js"])

        self.assertTestResult(False)
        self.assertInLog("TEST-UNEXPECTED-FAIL")
        self.assertInLog("failure.js")
        self.assertInLog("Error object")
        self.assertInLog("ERROR STACK")
        self.assertNotInLog("TEST-PASS")

    def testDoReportForeignObject(self):
        """
        Check that do_report_unexpected_exception produces reasonable messages when the
        input is a generic object with 'filename', 'message' and 'stack' attributes
        but 'object instanceof Error' returns false
        """
        self.writeFile("test_error.js", ADD_TEST_REPORT_OBJECT)
        self.writeManifest(["test_error.js"])

        self.assertTestResult(False)
        self.assertInLog("TEST-UNEXPECTED-FAIL")
        self.assertInLog("failure.js")
        self.assertInLog("Error object")
        self.assertInLog("ERROR STACK")
        self.assertNotInLog("TEST-PASS")

    def testDoReportRefError(self):
        """
        Check that do_report_unexpected_exception produces reasonable messages when the
        input is a JS-generated Error
        """
        self.writeFile("test_error.js", ADD_TEST_REPORT_REF_ERROR)
        self.writeManifest(["test_error.js"])

        self.assertTestResult(False)
        self.assertInLog("TEST-UNEXPECTED-FAIL")
        self.assertInLog("test_error.js")
        self.assertInLog("obj.noSuchFunction is not a function")
        self.assertInLog("run_test@")
        self.assertNotInLog("TEST-PASS")

    def testDoReportSyntaxError(self):
        """
        Check that attempting to load a test file containing a syntax error
        generates details of the error in the log
        """
        self.writeFile("test_error.js", LOAD_ERROR_SYNTAX_ERROR)
        self.writeManifest(["test_error.js"])

        self.assertTestResult(False)
        self.assertInLog("TEST-UNEXPECTED-FAIL")
        self.assertInLog("test_error.js")
        self.assertInLog("test_error.js contains SyntaxError")
        self.assertInLog("Diagnostic: SyntaxError: missing formal parameter at")
        self.assertInLog("test_error.js:3")
        self.assertNotInLog("TEST-PASS")

    def testDoReportNonSyntaxError(self):
        """
        Check that attempting to load a test file containing an error other
        than a syntax error generates details of the error in the log
        """
        self.writeFile("test_error.js", LOAD_ERROR_OTHER_ERROR)
        self.writeManifest(["test_error.js"])

        self.assertTestResult(False)
        self.assertInLog("TEST-UNEXPECTED-FAIL")
        self.assertInLog("Diagnostic: TypeError: generator function run_test returns a value at")
        self.assertInLog("test_error.js:4")
        self.assertNotInLog("TEST-PASS")

    def testAsyncCleanup(self):
        """
        Check that do_register_cleanup handles nicely cleanup tasks that
        return a promise
        """
        self.writeFile("test_asyncCleanup.js", ASYNC_CLEANUP)
        self.writeManifest(["test_asyncCleanup.js"])
        self.assertTestResult(False)
        self.assertInLog("\"1234\" == \"1234\"")
        self.assertInLog("At this stage, the test has succeeded")
        self.assertInLog("Throwing an error to force displaying the log")

if __name__ == "__main__":
    unittest.main()

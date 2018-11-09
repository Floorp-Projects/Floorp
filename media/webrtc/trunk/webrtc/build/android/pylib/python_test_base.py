# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class for Android Python-driven tests.

This test case is intended to serve as the base class for any Python-driven
tests. It is similar to the Python unitttest module in that the user's tests
inherit from this case and add their tests in that case.

When a PythonTestBase object is instantiated, its purpose is to run only one of
its tests. The test runner gives it the name of the test the instance will
run. The test runner calls SetUp with the Android device ID which the test will
run against. The runner runs the test method itself, collecting the result,
and calls TearDown.

Tests can basically do whatever they want in the test methods, such as call
Java tests using _RunJavaTests. Those methods have the advantage of massaging
the Java test results into Python test results.
"""

import logging
import os
import time

import android_commands
import apk_info
from run_java_tests import TestRunner
from test_result import SingleTestResult, TestResults


# aka the parent of com.google.android
BASE_ROOT = 'src' + os.sep


class PythonTestBase(object):
  """Base class for Python-driven tests."""

  def __init__(self, test_name):
    # test_name must match one of the test methods defined on a subclass which
    # inherits from this class.
    # It's stored so we can do the attr lookup on demand, allowing this class
    # to be pickled, a requirement for the multiprocessing module.
    self.test_name = test_name
    class_name = self.__class__.__name__
    self.qualified_name = class_name + '.' + self.test_name

  def SetUp(self, options):
    self.options = options
    self.shard_index = self.options.shard_index
    self.device_id = self.options.device_id
    self.adb = android_commands.AndroidCommands(self.device_id)
    self.ports_to_forward = []

  def TearDown(self):
    pass

  def Run(self):
    logging.warning('Running Python-driven test: %s', self.test_name)
    return getattr(self, self.test_name)()

  def _RunJavaTest(self, fname, suite, test):
    """Runs a single Java test with a Java TestRunner.

    Args:
      fname: filename for the test (e.g. foo/bar/baz/tests/FooTest.py)
      suite: name of the Java test suite (e.g. FooTest)
      test: name of the test method to run (e.g. testFooBar)

    Returns:
      TestResults object with a single test result.
    """
    test = self._ComposeFullTestName(fname, suite, test)
    apks = [apk_info.ApkInfo(self.options.test_apk_path,
            self.options.test_apk_jar_path)]
    java_test_runner = TestRunner(self.options, self.device_id, [test], False,
                                  self.shard_index,
                                  apks,
                                  self.ports_to_forward)
    return java_test_runner.Run()

  def _RunJavaTests(self, fname, tests):
    """Calls a list of tests and stops at the first test failure.

    This method iterates until either it encounters a non-passing test or it
    exhausts the list of tests. Then it returns the appropriate Python result.

    Args:
      fname: filename for the Python test
      tests: a list of Java test names which will be run

    Returns:
      A TestResults object containing a result for this Python test.
    """
    start_ms = int(time.time()) * 1000

    result = None
    for test in tests:
      # We're only running one test at a time, so this TestResults object will
      # hold only one result.
      suite, test_name = test.split('.')
      result = self._RunJavaTest(fname, suite, test_name)
      # A non-empty list means the test did not pass.
      if result.GetAllBroken():
        break

    duration_ms = int(time.time()) * 1000 - start_ms

    # Do something with result.
    return self._ProcessResults(result, start_ms, duration_ms)

  def _ProcessResults(self, result, start_ms, duration_ms):
    """Translates a Java test result into a Python result for this test.

    The TestRunner class that we use under the covers will return a test result
    for that specific Java test. However, to make reporting clearer, we have
    this method to abstract that detail and instead report that as a failure of
    this particular test case while still including the Java stack trace.

    Args:
      result: TestResults with a single Java test result
      start_ms: the time the test started
      duration_ms: the length of the test

    Returns:
      A TestResults object containing a result for this Python test.
    """
    test_results = TestResults()

    # If our test is in broken, then it crashed/failed.
    broken = result.GetAllBroken()
    if broken:
      # Since we have run only one test, take the first and only item.
      single_result = broken[0]

      log = single_result.log
      if not log:
        log = 'No logging information.'

      python_result = SingleTestResult(self.qualified_name, start_ms,
                                       duration_ms,
                                       log)

      # Figure out where the test belonged. There's probably a cleaner way of
      # doing this.
      if single_result in result.crashed:
        test_results.crashed = [python_result]
      elif single_result in result.failed:
        test_results.failed = [python_result]
      elif single_result in result.unknown:
        test_results.unknown = [python_result]

    else:
      python_result = SingleTestResult(self.qualified_name, start_ms,
                                       duration_ms)
      test_results.ok = [python_result]

    return test_results

  def _ComposeFullTestName(self, fname, suite, test):
    package_name = self._GetPackageName(fname)
    return package_name + '.' + suite + '#' + test

  def _GetPackageName(self, fname):
    """Extracts the package name from the test file path."""
    dirname = os.path.dirname(fname)
    package = dirname[dirname.rfind(BASE_ROOT) + len(BASE_ROOT):]
    return package.replace(os.sep, '.')

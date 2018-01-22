# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper module for calling python-based tests."""


import logging
import sys
import time

from test_result import TestResults


def CallPythonTest(test, options):
  """Invokes a test function and translates Python exceptions into test results.

  This method invokes SetUp()/TearDown() on the test. It is intended to be
  resilient to exceptions in SetUp(), the test itself, and TearDown(). Any
  Python exception means the test is marked as failed, and the test result will
  contain information about the exception.

  If SetUp() raises an exception, the test is not run.

  If TearDown() raises an exception, the test is treated as a failure. However,
  if the test itself raised an exception beforehand, that stack trace will take
  precedence whether or not TearDown() also raised an exception.

  shard_index is not applicable in single-device scenarios, when test execution
  is serial rather than parallel. Tests can use this to bring up servers with
  unique port numbers, for example. See also python_test_sharder.

  Args:
    test: an object which is ostensibly a subclass of PythonTestBase.
    options: Options to use for setting up tests.

  Returns:
    A TestResults object which contains any results produced by the test or, in
    the case of a Python exception, the Python exception info.
  """

  start_date_ms = int(time.time()) * 1000
  failed = False

  try:
    test.SetUp(options)
  except Exception:
    failed = True
    logging.exception(
        'Caught exception while trying to run SetUp() for test: ' +
        test.qualified_name)
    # Tests whose SetUp() method has failed are likely to fail, or at least
    # yield invalid results.
    exc_info = sys.exc_info()
    return TestResults.FromPythonException(test.qualified_name, start_date_ms,
                                           exc_info)

  try:
    result = test.Run()
  except Exception:
    # Setting this lets TearDown() avoid stomping on our stack trace from Run()
    # should TearDown() also raise an exception.
    failed = True
    logging.exception('Caught exception while trying to run test: ' +
                      test.qualified_name)
    exc_info = sys.exc_info()
    result = TestResults.FromPythonException(test.qualified_name, start_date_ms,
                                             exc_info)

  try:
    test.TearDown()
  except Exception:
    logging.exception(
        'Caught exception while trying run TearDown() for test: ' +
        test.qualified_name)
    if not failed:
      # Don't stomp the error during the test if TearDown blows up. This is a
      # trade-off: if the test fails, this will mask any problem with TearDown
      # until the test is fixed.
      exc_info = sys.exc_info()
      result = TestResults.FromPythonException(test.qualified_name,
                                               start_date_ms, exc_info)

  return result

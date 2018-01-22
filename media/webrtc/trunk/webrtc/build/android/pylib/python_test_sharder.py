# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Takes care of sharding the python-drive tests in multiple devices."""

import copy
import logging
import multiprocessing

from python_test_caller import CallPythonTest
from run_java_tests import FatalTestException
import sharded_tests_queue
from test_result import TestResults


def SetTestsContainer(tests_container):
  """Sets PythonTestSharder as a top-level field.

  PythonTestSharder uses multiprocessing.Pool, which creates a pool of
  processes. This is used to initialize each worker in the pool, ensuring that
  each worker has access to this shared pool of tests.

  The multiprocessing module requires that this be a top-level method.

  Args:
    tests_container: the container for all the tests.
  """
  PythonTestSharder.tests_container = tests_container


def _DefaultRunnable(test_runner):
  """A default runnable for a PythonTestRunner.

  Args:
    test_runner: A PythonTestRunner which will run tests.

  Returns:
    The test results.
  """
  return test_runner.RunTests()


class PythonTestRunner(object):
  """Thin wrapper around a list of PythonTestBase instances.

  This is meant to be a long-lived object which can run multiple Python tests
  within its lifetime. Tests will receive the device_id and shard_index.

  The shard index affords the ability to create unique port numbers (e.g.
  DEFAULT_PORT + shard_index) if the test so wishes.
  """

  def __init__(self, options):
    """Constructor.

    Args:
      options: Options to use for setting up tests.
    """
    self.options = options

  def RunTests(self):
    """Runs tests from the shared pool of tests, aggregating results.

    Returns:
      A list of test results for all of the tests which this runner executed.
    """
    tests = PythonTestSharder.tests_container

    results = []
    for t in tests:
      res = CallPythonTest(t, self.options)
      results.append(res)

    return TestResults.FromTestResults(results)


class PythonTestSharder(object):
  """Runs Python tests in parallel on multiple devices.

  This is lifted more or less wholesale from BaseTestRunner.

  Under the covers, it creates a pool of long-lived PythonTestRunners, which
  execute tests from the pool of tests.

  Args:
    attached_devices: a list of device IDs attached to the host.
    available_tests: a list of tests to run which subclass PythonTestBase.
    options: Options to use for setting up tests.

  Returns:
    An aggregated list of test results.
  """
  tests_container = None

  def __init__(self, attached_devices, available_tests, options):
    self.options = options
    self.attached_devices = attached_devices
    self.retries = options.shard_retries
    self.tests = available_tests

  def _SetupSharding(self, tests):
    """Creates the shared pool of tests and makes it available to test runners.

    Args:
      tests: the list of tests which will be consumed by workers.
    """
    SetTestsContainer(sharded_tests_queue.ShardedTestsQueue(
        len(self.attached_devices), tests))

  def RunShardedTests(self):
    """Runs tests in parallel using a pool of workers.

    Returns:
      A list of test results aggregated from all test runs.
    """
    logging.warning('*' * 80)
    logging.warning('Sharding in ' + str(len(self.attached_devices)) +
                    ' devices.')
    logging.warning('Note that the output is not synchronized.')
    logging.warning('Look for the "Final result" banner in the end.')
    logging.warning('*' * 80)
    all_passed = []
    test_results = TestResults()
    tests_to_run = self.tests
    for retry in xrange(self.retries):
      logging.warning('Try %d of %d', retry + 1, self.retries)
      self._SetupSharding(self.tests)
      test_runners = self._MakeTestRunners(self.attached_devices)
      logging.warning('Starting...')
      pool = multiprocessing.Pool(len(self.attached_devices),
                                  SetTestsContainer,
                                  [PythonTestSharder.tests_container])

      # List of TestResults objects from each test execution.
      try:
        results_lists = pool.map(_DefaultRunnable, test_runners)
      except Exception:
        logging.exception('Unable to run tests. Something with the '
                          'PythonTestRunners has gone wrong.')
        raise FatalTestException('PythonTestRunners were unable to run tests.')

      test_results = TestResults.FromTestResults(results_lists)
      # Accumulate passing results.
      all_passed += test_results.ok
      # If we have failed tests, map them to tests to retry.
      failed_tests = test_results.GetAllBroken()
      tests_to_run = self._GetTestsToRetry(self.tests,
                                           failed_tests)

      # Bail out early if we have no more tests. This can happen if all tests
      # pass before we're out of retries, for example.
      if not tests_to_run:
        break

    final_results = TestResults()
    # all_passed has accumulated all passing test results.
    # test_results will have the results from the most recent run, which could
    # include a variety of failure modes (unknown, crashed, failed, etc).
    final_results = test_results
    final_results.ok = all_passed

    return final_results

  def _MakeTestRunners(self, attached_devices):
    """Initialize and return a list of PythonTestRunners.

    Args:
      attached_devices: list of device IDs attached to host.

    Returns:
      A list of PythonTestRunners, one for each device.
    """
    test_runners = []
    for index, device in enumerate(attached_devices):
      logging.warning('*' * 80)
      logging.warning('Creating shard %d for %s', index, device)
      logging.warning('*' * 80)
      # Bind the PythonTestRunner to a device & shard index. Give it the
      # runnable which it will use to actually execute the tests.
      test_options = copy.deepcopy(self.options)
      test_options.ensure_value('device_id', device)
      test_options.ensure_value('shard_index', index)
      test_runner = PythonTestRunner(test_options)
      test_runners.append(test_runner)

    return test_runners

  def _GetTestsToRetry(self, available_tests, failed_tests):
    """Infers a list of tests to retry from failed tests and available tests.

    Args:
      available_tests: a list of tests which subclass PythonTestBase.
      failed_tests: a list of SingleTestResults representing failed tests.

    Returns:
      A list of test objects which correspond to test names found in
      failed_tests, or an empty list if there is no correspondence.
    """
    failed_test_names = map(lambda t: t.test_name, failed_tests)
    tests_to_retry = [t for t in available_tests
                      if t.qualified_name in failed_test_names]
    return tests_to_retry

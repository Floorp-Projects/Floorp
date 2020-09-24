# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import logging
import multiprocessing

from test_result import TestResults


def _ShardedTestRunnable(test):
  """Standalone function needed by multiprocessing.Pool."""
  log_format = '[' + test.device + '] # %(asctime)-15s: %(message)s'
  if logging.getLogger().handlers:
    logging.getLogger().handlers[0].setFormatter(logging.Formatter(log_format))
  else:
    logging.basicConfig(format=log_format)
  # Handle SystemExit here since python has a bug to exit current process
  try:
    return test.Run()
  except SystemExit:
    return TestResults()

def SetTestsContainer(tests_container):
  """Sets tests container.

  multiprocessing.Queue can't be pickled across processes, so we need to set
  this as a 'global', per process, via multiprocessing.Pool.
  """
  BaseTestSharder.tests_container = tests_container


class BaseTestSharder(object):
  """Base class for sharding tests across multiple devices.

  Args:
    attached_devices: A list of attached devices.
  """
  # See more in SetTestsContainer.
  tests_container = None

  def __init__(self, attached_devices):
    self.attached_devices = attached_devices
    self.retries = 1
    self.tests = []

  def CreateShardedTestRunner(self, device, index):
    """Factory function to create a suite-specific test runner.

    Args:
      device: Device serial where this shard will run
      index: Index of this device in the pool.

    Returns:
      An object of BaseTestRunner type (that can provide a "Run()" method).
    """
    pass

  def SetupSharding(self, tests):
    """Called before starting the shards."""
    pass

  def OnTestsCompleted(self, test_runners, test_results):
    """Notifies that we completed the tests."""
    pass

  def RunShardedTests(self):
    """Runs the tests in all connected devices.

    Returns:
      A TestResults object.
    """
    logging.warning('*' * 80)
    logging.warning('Sharding in ' + str(len(self.attached_devices)) +
                    ' devices.')
    logging.warning('Note that the output is not synchronized.')
    logging.warning('Look for the "Final result" banner in the end.')
    logging.warning('*' * 80)
    final_results = TestResults()
    for retry in xrange(self.retries):
      logging.warning('Try %d of %d', retry + 1, self.retries)
      self.SetupSharding(self.tests)
      test_runners = []
      for index, device in enumerate(self.attached_devices):
        logging.warning('*' * 80)
        logging.warning('Creating shard %d for %s', index, device)
        logging.warning('*' * 80)
        test_runner = self.CreateShardedTestRunner(device, index)
        test_runners += [test_runner]
      logging.warning('Starting...')
      pool = multiprocessing.Pool(len(self.attached_devices),
                                  SetTestsContainer,
                                  [BaseTestSharder.tests_container])
      # map can't handle KeyboardInterrupt exception. It's a python bug.
      # So use map_async instead.
      async_results = pool.map_async(_ShardedTestRunnable, test_runners)
      results_lists = async_results.get(999999)
      test_results = TestResults.FromTestResults(results_lists)
      if retry == self.retries - 1:
        all_passed = final_results.ok + test_results.ok
        final_results = test_results
        final_results.ok = all_passed
        break
      else:
        final_results.ok += test_results.ok
        self.tests = []
        for t in test_results.GetAllBroken():
          self.tests += [t.name]
        if not self.tests:
          break
    self.OnTestsCompleted(test_runners, final_results)
    return final_results

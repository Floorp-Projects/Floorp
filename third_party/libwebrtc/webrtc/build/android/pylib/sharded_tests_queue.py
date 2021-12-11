# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""A module that contains a queue for running sharded tests."""

import multiprocessing


class ShardedTestsQueue(object):
  """A queue for managing pending tests across different runners.

  This class should only be used when sharding.

  Attributes:
    num_devices: an integer; the number of attached Android devices.
    tests: a list of tests to be run.
    tests_queue: if sharding, a JoinableQueue object that holds tests from
        |tests|. Otherwise, a list holding tests.
    results_queue: a Queue object to hold TestResults objects.
  """
  _STOP_SENTINEL = 'STOP'  # sentinel value for iter()

  def __init__(self, num_devices, tests):
    self.num_devices = num_devices
    self.tests_queue = multiprocessing.Queue()
    for test in tests:
      self.tests_queue.put(test)
    for _ in xrange(self.num_devices):
      self.tests_queue.put(ShardedTestsQueue._STOP_SENTINEL)

  def __iter__(self):
    """Returns an iterator with the test cases."""
    return iter(self.tests_queue.get, ShardedTestsQueue._STOP_SENTINEL)

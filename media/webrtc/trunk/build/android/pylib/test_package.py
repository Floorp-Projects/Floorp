# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import logging
import re
import os
import pexpect

from perf_tests_helper import PrintPerfResult
from test_result import BaseTestResult, TestResults


# TODO(bulach): TestPackage, TestPackageExecutable and
# TestPackageApk are a work in progress related to making the native tests
# run as a NDK-app from an APK rather than a stand-alone executable.
class TestPackage(object):
  """A helper base class for both APK and stand-alone executables.

  Args:
    adb: ADB interface the tests are using.
    device: Device to run the tests.
    test_suite: A specific test suite to run, empty to run all.
    timeout: Timeout for each test.
    rebaseline: Whether or not to run tests in isolation and update the filter.
    performance_test: Whether or not performance test(s).
    cleanup_test_files: Whether or not to cleanup test files on device.
    tool: Name of the Valgrind tool.
    dump_debug_info: A debug_info object.
  """

  def __init__(self, adb, device, test_suite, timeout, rebaseline,
               performance_test, cleanup_test_files, tool, dump_debug_info):
    self.adb = adb
    self.device = device
    self.test_suite_full = test_suite
    self.test_suite = os.path.splitext(test_suite)[0]
    self.test_suite_basename = self._GetTestSuiteBaseName()
    self.test_suite_dirname = os.path.dirname(
        self.test_suite.split(self.test_suite_basename)[0]);
    self.rebaseline = rebaseline
    self.performance_test = performance_test
    self.cleanup_test_files = cleanup_test_files
    self.tool = tool
    if timeout == 0:
      timeout = 60
    # On a VM (e.g. chromium buildbots), this timeout is way too small.
    if os.environ.get('BUILDBOT_SLAVENAME'):
      timeout = timeout * 2
    self.timeout = timeout * self.tool.GetTimeoutScale()
    self.dump_debug_info = dump_debug_info

  def _BeginGetIOStats(self):
    """Gets I/O statistics before running test.

    Return:
      I/O stats object.The I/O stats object may be None if the test is not
      performance test.
    """
    initial_io_stats = None
    # Try to get the disk I/O statistics for all performance tests.
    if self.performance_test and not self.rebaseline:
      initial_io_stats = self.adb.GetIoStats()
    return initial_io_stats

  def _EndGetIOStats(self, initial_io_stats):
    """Gets I/O statistics after running test and calcuate the I/O delta.

    Args:
      initial_io_stats: I/O stats object got from _BeginGetIOStats.

    Return:
      String for formated diso I/O statistics.
    """
    disk_io = ''
    if self.performance_test and initial_io_stats:
      final_io_stats = self.adb.GetIoStats()
      for stat in final_io_stats:
        disk_io += '\n' + PrintPerfResult(stat, stat,
                                          [final_io_stats[stat] -
                                           initial_io_stats[stat]],
                                          stat.split('_')[1],
                                          print_to_stdout=False)
      logging.info(disk_io)
    return disk_io

  def GetDisabledPrefixes(self):
    return ['DISABLED_', 'FLAKY_', 'FAILS_']

  def _ParseGTestListTests(self, all_tests):
    ret = []
    current = ''
    disabled_prefixes = self.GetDisabledPrefixes()
    for test in all_tests:
      if not test:
        continue
      if test[0] != ' ' and test.endswith('.'):
        current = test
        continue
      if 'YOU HAVE' in test:
        break
      test_name = test[2:]
      if not any([test_name.startswith(x) for x in disabled_prefixes]):
        ret += [current + test_name]
    return ret

  def PushDataAndPakFiles(self):
    if (self.test_suite_basename == 'ui_unittests' or
        self.test_suite_basename == 'unit_tests'):
      self.adb.PushIfNeeded(
          self.test_suite_dirname + '/chrome.pak',
          '/data/local/tmp/paks/chrome.pak')
      self.adb.PushIfNeeded(
          self.test_suite_dirname + '/locales/en-US.pak',
          '/data/local/tmp/paks/en-US.pak')
    if self.test_suite_basename == 'unit_tests':
      self.adb.PushIfNeeded(
          self.test_suite_dirname + '/resources.pak',
          '/data/local/tmp/paks/resources.pak')
      self.adb.PushIfNeeded(
          self.test_suite_dirname + '/chrome_100_percent.pak',
          '/data/local/tmp/paks/chrome_100_percent.pak')
      self.adb.PushIfNeeded(self.test_suite_dirname + '/test_data',
                            '/data/local/tmp/test_data')
    if self.test_suite_basename == 'content_unittests':
      self.adb.PushIfNeeded(
          self.test_suite_dirname + '/content_resources.pak',
          '/data/local/tmp/paks/content_resources.pak')

  def _WatchTestOutput(self, p):
    """Watches the test output.
    Args:
      p: the process generating output as created by pexpect.spawn.
    """
    ok_tests = []
    failed_tests = []
    crashed_tests = []
    timed_out = False
    overall_fail = False
    re_run = re.compile('\[ RUN      \] ?(.*)\r\n')
    # APK tests rely on the END tag.
    re_end = re.compile('\[ END      \] ?(.*)\r\n')
    # Signal handlers are installed before starting tests
    # to output the CRASHED marker when a crash happens.
    re_crash = re.compile('\[ CRASHED      \](.*)\r\n')
    re_fail = re.compile('\[  FAILED  \] ?(.*)\r\n')
    re_runner_fail = re.compile('\[ RUNNER_FAILED \] ?(.*)\r\n')
    re_ok = re.compile('\[       OK \] ?(.*)\r\n')
    io_stats_before = self._BeginGetIOStats()
    while True:
      found = p.expect([re_run, pexpect.EOF, re_end, re_runner_fail],
                       timeout=self.timeout)
      if found == 1:  # matched pexpect.EOF
        break
      if found == 2:  # matched END.
        break
      if found == 3:  # RUNNER_FAILED
        logging.error('RUNNER_FAILED')
        overall_fail = True
        break
      if self.dump_debug_info:
        self.dump_debug_info.TakeScreenshot('_Test_Start_Run_')
      full_test_name = p.match.group(1)
      found = p.expect([re_ok, re_fail, re_crash, pexpect.EOF, pexpect.TIMEOUT],
                       timeout=self.timeout)
      if found == 0:  # re_ok
        ok_tests += [BaseTestResult(full_test_name.replace('\r', ''),
                                    p.before)]
        continue
      if found == 2: # re_crash
        crashed_tests += [BaseTestResult(full_test_name.replace('\r', ''),
                                         p.before)]
        overall_fail = True
        break
      # The test failed.
      failed_tests += [BaseTestResult(full_test_name.replace('\r', ''),
                                      p.before)]
      if found >= 3:
        # The test bailed out (i.e., didn't print OK or FAIL).
        if found == 4:  # pexpect.TIMEOUT
          logging.error('Test terminated after %d second timeout.',
                        self.timeout)
          timed_out = True
        break
    p.close()
    if not self.rebaseline:
      ok_tests += self._EndGetIOStats(io_stats_before)
      ret_code = self._GetGTestReturnCode()
      if ret_code:
        failed_tests += [BaseTestResult('gtest exit code: %d' % ret_code,
                                        'pexpect.before: %s'
                                        '\npexpect.after: %s'
                                        % (p.before,
                                           p.after))]
    # Create TestResults and return
    return TestResults.FromRun(ok=ok_tests, failed=failed_tests,
                               crashed=crashed_tests, timed_out=timed_out,
                               overall_fail=overall_fail)

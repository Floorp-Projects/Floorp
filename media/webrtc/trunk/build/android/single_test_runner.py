# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

from base_test_runner import BaseTestRunner
import debug_info
import run_tests_helper
from test_package_executable import TestPackageExecutable
from test_result import TestResults


class SingleTestRunner(BaseTestRunner):
  """Single test suite attached to a single device.

  Args:
    device: Device to run the tests.
    test_suite: A specific test suite to run, empty to run all.
    gtest_filter: A gtest_filter flag.
    test_arguments: Additional arguments to pass to the test binary.
    timeout: Timeout for each test.
    rebaseline: Whether or not to run tests in isolation and update the filter.
    performance_test: Whether or not performance test(s).
    cleanup_test_files: Whether or not to cleanup test files on device.
    tool: Name of the Valgrind tool.
    dump_debug_info: Whether or not to dump debug information.
  """

  def __init__(self, device, test_suite, gtest_filter, test_arguments, timeout,
               rebaseline, performance_test, cleanup_test_files, tool,
               dump_debug_info=False,
               fast_and_loose=False):
    BaseTestRunner.__init__(self, device)
    self._running_on_emulator = self.device.startswith('emulator')
    self._gtest_filter = gtest_filter
    self._test_arguments = test_arguments
    self.test_results = TestResults()
    if dump_debug_info:
      self.dump_debug_info = debug_info.GTestDebugInfo(self.adb, device,
           os.path.basename(test_suite), gtest_filter)
    else:
      self.dump_debug_info = None
    self.fast_and_loose = fast_and_loose

    self.test_package = TestPackageExecutable(self.adb, device,
        test_suite, timeout, rebaseline, performance_test, cleanup_test_files,
        tool, self.dump_debug_info)

  def _GetHttpServerDocumentRootForTestSuite(self):
    """Returns the document root needed by the test suite."""
    if self.test_package.test_suite_basename == 'page_cycler_tests':
      return os.path.join(run_tests_helper.CHROME_DIR, 'data', 'page_cycler')
    return None


  def _TestSuiteRequiresMockTestServer(self):
    """Returns True if the test suite requires mock test server."""
    return False
  # TODO(yfriedman): Disabled because of flakiness.
  # (self.test_package.test_suite_basename == 'unit_tests' or
  #          self.test_package.test_suite_basename == 'net_unittests' or
  #          False)

  def _GetFilterFileName(self):
    """Returns the filename of gtest filter."""
    filter_dir = os.path.join(sys.path[0], 'gtest_filter')
    filter_name = self.test_package.test_suite_basename + '_disabled'
    disabled_filter = os.path.join(filter_dir, filter_name)
    return disabled_filter

  def _GetAdditionalEmulatorFilterName(self):
    """Returns the filename of additional gtest filter for emulator."""
    filter_dir = os.path.join(sys.path[0], 'gtest_filter')
    filter_name = '%s%s' % (self.test_package.test_suite_basename,
        '_emulator_additional_disabled')
    disabled_filter = os.path.join(filter_dir, filter_name)
    return disabled_filter

  def GetDisabledTests(self):
    """Returns a list of disabled tests.

    Returns:
      A list of disabled tests obtained from gtest_filter/test_suite_disabled.
    """
    disabled_tests = run_tests_helper.GetExpectations(self._GetFilterFileName())
    if self._running_on_emulator:
      # Append emulator's filter file.
      disabled_tests.extend(run_tests_helper.GetExpectations(
          self._GetAdditionalEmulatorFilterName()))
    return disabled_tests

  def UpdateFilter(self, failed_tests):
    """Updates test_suite_disabled file with the new filter (deletes if empty).

    If running in Emulator, only the failed tests which are not in the normal
    filter returned by _GetFilterFileName() are written to emulator's
    additional filter file.

    Args:
      failed_tests: A sorted list of failed tests.
    """
    disabled_tests = []
    if not self._running_on_emulator:
      filter_file_name = self._GetFilterFileName()
    else:
      filter_file_name = self._GetAdditionalEmulatorFilterName()
      disabled_tests.extend(
          run_tests_helper.GetExpectations(self._GetFilterFileName()))
      logging.info('About to update emulator\'s addtional filter (%s).'
          % filter_file_name)

    new_failed_tests = []
    if failed_tests:
      for test in failed_tests:
        if test.name not in disabled_tests:
          new_failed_tests.append(test.name)

    if not new_failed_tests:
      if os.path.exists(filter_file_name):
        os.unlink(filter_file_name)
      return

    filter_file = file(filter_file_name, 'w')
    if self._running_on_emulator:
      filter_file.write('# Addtional list of suppressions from emulator\n')
    else:
      filter_file.write('# List of suppressions\n')
    filter_file.write("""This file was automatically generated by run_tests.py
        """)
    filter_file.write('\n'.join(sorted(new_failed_tests)))
    filter_file.write('\n')
    filter_file.close()

  def GetDataFilesForTestSuite(self):
    """Returns a list of data files/dirs needed by the test suite."""
    # Ideally, we'd just push all test data. However, it has >100MB, and a lot
    # of the files are not relevant (some are used for browser_tests, others for
    # features not supported, etc..).
    if self.test_package.test_suite_basename in ['base_unittests',
                                                 'sql_unittests',
                                                 'unit_tests']:
      return [
          'net/data/cache_tests/insert_load1',
          'net/data/cache_tests/dirty_entry5',
          'ui/base/test/data/data_pack_unittest',
          'chrome/test/data/bookmarks/History_with_empty_starred',
          'chrome/test/data/bookmarks/History_with_starred',
          'chrome/test/data/extensions/json_schema_test.js',
          'chrome/test/data/History/',
          'chrome/test/data/json_schema_validator/',
          'chrome/test/data/serializer_nested_test.js',
          'chrome/test/data/serializer_test.js',
          'chrome/test/data/serializer_test_nowhitespace.js',
          'chrome/test/data/top_sites/',
          'chrome/test/data/web_database',
          'chrome/test/data/zip',
          ]
    elif self.test_package.test_suite_basename == 'net_unittests':
      return [
          'net/data/cache_tests',
          'net/data/filter_unittests',
          'net/data/ftp',
          'net/data/proxy_resolver_v8_unittest',
          'net/data/ssl/certificates',
          ]
    elif self.test_package.test_suite_basename == 'ui_tests':
      return [
          'chrome/test/data/dromaeo',
          'chrome/test/data/json2.js',
          'chrome/test/data/sunspider',
          'chrome/test/data/v8_benchmark',
          'chrome/test/ui/sunspider_uitest.js',
          'chrome/test/ui/v8_benchmark_uitest.js',
          ]
    elif self.test_package.test_suite_basename == 'page_cycler_tests':
      data = [
          'tools/page_cycler',
          'data/page_cycler',
          ]
      for d in data:
        if not os.path.exists(d):
          raise Exception('Page cycler data not found.')
      return data
    elif self.test_package.test_suite_basename == 'webkit_unit_tests':
      return [
          'third_party/WebKit/Source/WebKit/chromium/tests/data',
          ]
    return []

  def LaunchHelperToolsForTestSuite(self):
    """Launches helper tools for the test suite.

    Sometimes one test may need to run some helper tools first in order to
    successfully complete the test.
    """
    document_root = self._GetHttpServerDocumentRootForTestSuite()
    if document_root:
      self.LaunchTestHttpServer(document_root)
    if self._TestSuiteRequiresMockTestServer():
      self.LaunchChromeTestServerSpawner()

  def StripAndCopyFiles(self):
    """Strips and copies the required data files for the test suite."""
    self.test_package.StripAndCopyExecutable()
    self.test_package.tool.CopyFiles()
    test_data = self.GetDataFilesForTestSuite()
    if test_data and not self.fast_and_loose:
      if self.test_package.test_suite_basename == 'page_cycler_tests':
        # Since the test data for page cycler are huge (around 200M), we use
        # sdcard to store the data and create symbol links to map them to
        # data/local/tmp/ later.
        self.CopyTestData(test_data, '/sdcard/')
        for p in [os.path.dirname(d) for d in test_data if os.path.isdir(d)]:
          mapped_device_path = '/data/local/tmp/' + p
          # Unlink the mapped_device_path at first in case it was mapped to
          # a wrong path. Add option '-r' becuase the old path could be a dir.
          self.adb.RunShellCommand('rm -r %s' %  mapped_device_path)
          self.adb.RunShellCommand(
              'ln -s /sdcard/%s %s' % (p, mapped_device_path))
      else:
        self.CopyTestData(test_data, '/data/local/tmp/')

  def RunTestsWithFilter(self):
    """Runs a tests via a small, temporary shell script."""
    self.test_package.CreateTestRunnerScript(self._gtest_filter,
                                            self._test_arguments)
    self.test_results = self.test_package.RunTestsAndListResults()

  def RebaselineTests(self):
    """Runs all available tests, restarting in case of failures."""
    if self._gtest_filter:
      all_tests = set(self._gtest_filter.split(':'))
    else:
      all_tests = set(self.test_package.GetAllTests())
    failed_results = set()
    executed_results = set()
    while True:
      executed_names = set([f.name for f in executed_results])
      self._gtest_filter = ':'.join(all_tests - executed_names)
      self.RunTestsWithFilter()
      failed_results.update(self.test_results.crashed,
          self.test_results.failed)
      executed_results.update(self.test_results.crashed,
                              self.test_results.failed,
                              self.test_results.ok)
      executed_names = set([f.name for f in executed_results])
      logging.info('*' * 80)
      logging.info(self.device)
      logging.info('Executed: ' + str(len(executed_names)) + ' of ' +
                   str(len(all_tests)))
      logging.info('Failed so far: ' + str(len(failed_results)) + ' ' +
                   str([f.name for f in failed_results]))
      logging.info('Remaining: ' + str(len(all_tests - executed_names)) + ' ' +
                   str(all_tests - executed_names))
      logging.info('*' * 80)
      if executed_names == all_tests:
        break
    self.test_results = TestResults.FromOkAndFailed(list(executed_results -
                                                         failed_results),
                                                    list(failed_results))

  def _RunTestsForSuiteInternal(self):
    """Runs all tests (in rebaseline mode, run each test in isolation).

    Returns:
      A TestResults object.
    """
    if self.test_package.rebaseline:
      self.RebaselineTests()
    else:
      if not self._gtest_filter:
        self._gtest_filter = ('-' + ':'.join(self.GetDisabledTests()) + ':' +
                             ':'.join(['*.' + x + '*' for x in
                                     self.test_package.GetDisabledPrefixes()]))
      self.RunTestsWithFilter()

  def SetUp(self):
    """Sets up necessary test enviroment for the test suite."""
    super(SingleTestRunner, self).SetUp()
    if self.test_package.performance_test:
      if run_tests_helper.IsRunningAsBuildbot():
        self.adb.SetJavaAssertsEnabled(enable=False)
        self.adb.Reboot(full_reboot=False)
      self.adb.SetupPerformanceTest()
    if self.dump_debug_info:
      self.dump_debug_info.StartRecordingLog(True)
    self.StripAndCopyFiles()
    self.LaunchHelperToolsForTestSuite()
    self.test_package.tool.SetupEnvironment()

  def TearDown(self):
    """Cleans up the test enviroment for the test suite."""
    super(SingleTestRunner, self).TearDown()
    self.test_package.tool.CleanUpEnvironment()
    if self.test_package.cleanup_test_files:
      self.adb.RemovePushedFiles()
    if self.dump_debug_info:
      self.dump_debug_info.StopRecordingLog()
    if self.test_package.performance_test:
      self.adb.TearDownPerformanceTest()

  def RunTests(self):
    """Runs the tests and cleans up the files once finished.

    Returns:
      A TestResults object.
    """
    self.SetUp()
    try:
      self._RunTestsForSuiteInternal()
    finally:
      self.TearDown()
    return self.test_results

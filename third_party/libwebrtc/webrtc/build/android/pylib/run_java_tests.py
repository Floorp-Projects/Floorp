# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the Java tests. See more information on run_instrumentation_tests.py."""

import fnmatch
import logging
import os
import re
import shutil
import sys
import time

import android_commands
import apk_info
from base_test_runner import BaseTestRunner
from base_test_sharder import BaseTestSharder, SetTestsContainer
import cmd_helper
import constants
import errors
from forwarder import Forwarder
from json_perf_parser import GetAverageRunInfoFromJSONString
from perf_tests_helper import PrintPerfResult
import sharded_tests_queue
from test_result import SingleTestResult, TestResults
import valgrind_tools

_PERF_TEST_ANNOTATION = 'PerfTest'


class FatalTestException(Exception):
  """A fatal test exception."""
  pass


def _TestNameToExpectation(test_name):
  # A test name is a Package.Path.Class#testName; convert to what we use in
  # the expectation file.
  return '.'.join(test_name.replace('#', '.').split('.')[-2:])


def FilterTests(test_names, pattern_list, inclusive):
  """Filters |test_names| using a list of patterns.

  Args:
    test_names: A list of test names.
    pattern_list: A list of patterns.
    inclusive: If True, returns the tests that match any pattern. if False,
               returns the tests that do not match any pattern.
  Returns:
    A list of test names.
  """
  ret = []
  for t in test_names:
    has_match = False
    for pattern in pattern_list:
      has_match = has_match or fnmatch.fnmatch(_TestNameToExpectation(t),
                                               pattern)
    if has_match == inclusive:
      ret += [t]
  return ret


class TestRunner(BaseTestRunner):
  """Responsible for running a series of tests connected to a single device."""

  _DEVICE_DATA_DIR = 'chrome/test/data'
  _EMMA_JAR = os.path.join(os.environ.get('ANDROID_BUILD_TOP', ''),
                           'external/emma/lib/emma.jar')
  _COVERAGE_MERGED_FILENAME = 'unittest_coverage.es'
  _COVERAGE_WEB_ROOT_DIR = os.environ.get('EMMA_WEB_ROOTDIR')
  _COVERAGE_FILENAME = 'coverage.ec'
  _COVERAGE_RESULT_PATH = ('/data/data/com.google.android.apps.chrome/files/' +
                           _COVERAGE_FILENAME)
  _COVERAGE_META_INFO_PATH = os.path.join(os.environ.get('ANDROID_BUILD_TOP',
                                                         ''),
                                          'out/target/common/obj/APPS',
                                          'Chrome_intermediates/coverage.em')
  _HOSTMACHINE_PERF_OUTPUT_FILE = '/tmp/chrome-profile'
  _DEVICE_PERF_OUTPUT_SEARCH_PREFIX = (constants.DEVICE_PERF_OUTPUT_DIR +
                                       '/chrome-profile*')
  _DEVICE_HAS_TEST_FILES = {}

  def __init__(self, options, device, tests_iter, coverage, shard_index, apks,
               ports_to_forward):
    """Create a new TestRunner.

    Args:
      options: An options object with the following required attributes:
      -  build_type: 'Release' or 'Debug'.
      -  install_apk: Re-installs the apk if opted.
      -  save_perf_json: Whether or not to save the JSON file from UI perf
            tests.
      -  screenshot_failures: Take a screenshot for a test failure
      -  tool: Name of the Valgrind tool.
      -  wait_for_debugger: blocks until the debugger is connected.
      device: Attached android device.
      tests_iter: A list of tests to be run.
      coverage: Collects coverage information if opted.
      shard_index: shard # for this TestRunner, used to create unique port
          numbers.
      apks: A list of ApkInfo objects need to be installed. The first element
            should be the tests apk, the rests could be the apks used in test.
            The default is ChromeTest.apk.
      ports_to_forward: A list of port numbers for which to set up forwarders.
                        Can be optionally requested by a test case.
    Raises:
      FatalTestException: if coverage metadata is not available.
    """
    BaseTestRunner.__init__(
        self, device, options.tool, shard_index, options.build_type)

    if not apks:
      apks = [apk_info.ApkInfo(options.test_apk_path,
                               options.test_apk_jar_path)]

    self.build_type = options.build_type
    self.install_apk = options.install_apk
    self.save_perf_json = options.save_perf_json
    self.screenshot_failures = options.screenshot_failures
    self.wait_for_debugger = options.wait_for_debugger

    self.tests_iter = tests_iter
    self.coverage = coverage
    self.apks = apks
    self.test_apk = apks[0]
    self.instrumentation_class_path = self.test_apk.GetPackageName()
    self.ports_to_forward = ports_to_forward

    self.test_results = TestResults()
    self.forwarder = None

    if self.coverage:
      if os.path.exists(TestRunner._COVERAGE_MERGED_FILENAME):
        os.remove(TestRunner._COVERAGE_MERGED_FILENAME)
      if not os.path.exists(TestRunner._COVERAGE_META_INFO_PATH):
        raise FatalTestException('FATAL ERROR in ' + sys.argv[0] +
                                 ' : Coverage meta info [' +
                                 TestRunner._COVERAGE_META_INFO_PATH +
                                 '] does not exist.')
      if (not TestRunner._COVERAGE_WEB_ROOT_DIR or
          not os.path.exists(TestRunner._COVERAGE_WEB_ROOT_DIR)):
        raise FatalTestException('FATAL ERROR in ' + sys.argv[0] +
                                 ' : Path specified in $EMMA_WEB_ROOTDIR [' +
                                 TestRunner._COVERAGE_WEB_ROOT_DIR +
                                 '] does not exist.')

  def _GetTestsIter(self):
    if not self.tests_iter:
      # multiprocessing.Queue can't be pickled across processes if we have it as
      # a member set during constructor.  Grab one here instead.
      self.tests_iter = (BaseTestSharder.tests_container)
    assert self.tests_iter
    return self.tests_iter

  def CopyTestFilesOnce(self):
    """Pushes the test data files to the device. Installs the apk if opted."""
    if TestRunner._DEVICE_HAS_TEST_FILES.get(self.device, False):
      logging.warning('Already copied test files to device %s, skipping.',
                      self.device)
      return
    host_test_files = [
        ('android_webview/test/data/device_files', 'webview'),
        ('content/test/data/android/device_files', 'content'),
        ('chrome/test/data/android/device_files', 'chrome')
    ]
    for (host_src, dst_layer) in host_test_files:
      host_test_files_path = constants.CHROME_DIR + '/' + host_src
      if os.path.exists(host_test_files_path):
        self.adb.PushIfNeeded(host_test_files_path,
                              self.adb.GetExternalStorage() + '/' +
                              TestRunner._DEVICE_DATA_DIR + '/' + dst_layer)
    if self.install_apk:
      for apk in self.apks:
        self.adb.ManagedInstall(apk.GetApkPath(),
                                package_name=apk.GetPackageName())
    self.tool.CopyFiles()
    TestRunner._DEVICE_HAS_TEST_FILES[self.device] = True

  def SaveCoverageData(self, test):
    """Saves the Emma coverage data before it's overwritten by the next test.

    Args:
      test: the test whose coverage data is collected.
    """
    if not self.coverage:
      return
    if not self.adb.Adb().Pull(TestRunner._COVERAGE_RESULT_PATH,
                               constants.CHROME_DIR):
      logging.error('ERROR: Unable to find file ' +
                    TestRunner._COVERAGE_RESULT_PATH +
                    ' on the device for test ' + test)
    pulled_coverage_file = os.path.join(constants.CHROME_DIR,
                                        TestRunner._COVERAGE_FILENAME)
    if os.path.exists(TestRunner._COVERAGE_MERGED_FILENAME):
      cmd = ['java', '-classpath', TestRunner._EMMA_JAR, 'emma', 'merge',
             '-in', pulled_coverage_file,
             '-in', TestRunner._COVERAGE_MERGED_FILENAME,
             '-out', TestRunner._COVERAGE_MERGED_FILENAME]
      cmd_helper.RunCmd(cmd)
    else:
      shutil.copy(pulled_coverage_file,
                  TestRunner._COVERAGE_MERGED_FILENAME)
    os.remove(pulled_coverage_file)

  def GenerateCoverageReportIfNeeded(self):
    """Uses the Emma to generate a coverage report and a html page."""
    if not self.coverage:
      return
    cmd = ['java', '-classpath', TestRunner._EMMA_JAR,
           'emma', 'report', '-r', 'html',
           '-in', TestRunner._COVERAGE_MERGED_FILENAME,
           '-in', TestRunner._COVERAGE_META_INFO_PATH]
    cmd_helper.RunCmd(cmd)
    new_dir = os.path.join(TestRunner._COVERAGE_WEB_ROOT_DIR,
                           time.strftime('Coverage_for_%Y_%m_%d_%a_%H:%M'))
    shutil.copytree('coverage', new_dir)

    latest_dir = os.path.join(TestRunner._COVERAGE_WEB_ROOT_DIR,
                              'Latest_Coverage_Run')
    if os.path.exists(latest_dir):
      shutil.rmtree(latest_dir)
    os.mkdir(latest_dir)
    webserver_new_index = os.path.join(new_dir, 'index.html')
    webserver_new_files = os.path.join(new_dir, '_files')
    webserver_latest_index = os.path.join(latest_dir, 'index.html')
    webserver_latest_files = os.path.join(latest_dir, '_files')
    # Setup new softlinks to last result.
    os.symlink(webserver_new_index, webserver_latest_index)
    os.symlink(webserver_new_files, webserver_latest_files)
    cmd_helper.RunCmd(['chmod', '755', '-R', latest_dir, new_dir])

  def _GetInstrumentationArgs(self):
    ret = {}
    if self.coverage:
      ret['coverage'] = 'true'
    if self.wait_for_debugger:
      ret['debug'] = 'true'
    return ret

  def _TakeScreenshot(self, test):
    """Takes a screenshot from the device."""
    screenshot_tool = os.path.join(constants.CHROME_DIR,
        'third_party/android_tools/sdk/tools/monkeyrunner')
    screenshot_script = os.path.join(constants.CHROME_DIR,
        'build/android/monkeyrunner_screenshot.py')
    screenshot_path = os.path.join(constants.CHROME_DIR,
                                   'out_screenshots')
    if not os.path.exists(screenshot_path):
      os.mkdir(screenshot_path)
    screenshot_name = os.path.join(screenshot_path, test + '.png')
    logging.info('Taking screenshot named %s', screenshot_name)
    cmd_helper.RunCmd([screenshot_tool, screenshot_script,
                       '--serial', self.device,
                       '--file', screenshot_name])

  def SetUp(self):
    """Sets up the test harness and device before all tests are run."""
    super(TestRunner, self).SetUp()
    if not self.adb.IsRootEnabled():
      logging.warning('Unable to enable java asserts for %s, non rooted device',
                      self.device)
    else:
      if self.adb.SetJavaAssertsEnabled(enable=True):
        self.adb.Reboot(full_reboot=False)

    # We give different default value to launch HTTP server based on shard index
    # because it may have race condition when multiple processes are trying to
    # launch lighttpd with same port at same time.
    http_server_ports = self.LaunchTestHttpServer(
        os.path.join(constants.CHROME_DIR),
        (constants.LIGHTTPD_RANDOM_PORT_FIRST + self.shard_index))
    if self.ports_to_forward:
      port_pairs = [(port, port) for port in self.ports_to_forward]
      # We need to remember which ports the HTTP server is using, since the
      # forwarder will stomp on them otherwise.
      port_pairs.append(http_server_ports)
      self.forwarder = Forwarder(
         self.adb, port_pairs, self.tool, '127.0.0.1', self.build_type)
    self.CopyTestFilesOnce()
    self.flags.AddFlags(['--enable-test-intents'])

  def TearDown(self):
    """Cleans up the test harness and saves outstanding data from test run."""
    if self.forwarder:
      self.forwarder.Close()
    self.GenerateCoverageReportIfNeeded()
    super(TestRunner, self).TearDown()

  def TestSetup(self, test):
    """Sets up the test harness for running a particular test.

    Args:
      test: The name of the test that will be run.
    """
    self.SetupPerfMonitoringIfNeeded(test)
    self._SetupIndividualTestTimeoutScale(test)
    self.tool.SetupEnvironment()

    # Make sure the forwarder is still running.
    self.RestartHttpServerForwarderIfNecessary()

  def _IsPerfTest(self, test):
    """Determines whether a test is a performance test.

    Args:
      test: The name of the test to be checked.

    Returns:
      Whether the test is annotated as a performance test.
    """
    return _PERF_TEST_ANNOTATION in self.test_apk.GetTestAnnotations(test)

  def SetupPerfMonitoringIfNeeded(self, test):
    """Sets up performance monitoring if the specified test requires it.

    Args:
      test: The name of the test to be run.
    """
    if not self._IsPerfTest(test):
      return
    self.adb.Adb().SendCommand('shell rm ' +
                               TestRunner._DEVICE_PERF_OUTPUT_SEARCH_PREFIX)
    self.adb.StartMonitoringLogcat()

  def TestTeardown(self, test, test_result):
    """Cleans up the test harness after running a particular test.

    Depending on the options of this TestRunner this might handle coverage
    tracking or performance tracking.  This method will only be called if the
    test passed.

    Args:
      test: The name of the test that was just run.
      test_result: result for this test.
    """

    self.tool.CleanUpEnvironment()

    # The logic below relies on the test passing.
    if not test_result or test_result.GetStatusCode():
      return

    self.TearDownPerfMonitoring(test)
    self.SaveCoverageData(test)

  def TearDownPerfMonitoring(self, test):
    """Cleans up performance monitoring if the specified test required it.

    Args:
      test: The name of the test that was just run.
    Raises:
      FatalTestException: if there's anything wrong with the perf data.
    """
    if not self._IsPerfTest(test):
      return
    raw_test_name = test.split('#')[1]

    # Wait and grab annotation data so we can figure out which traces to parse
    regex = self.adb.WaitForLogMatch(re.compile('\*\*PERFANNOTATION\(' +
                                                raw_test_name +
                                                '\)\:(.*)'), None)

    # If the test is set to run on a specific device type only (IE: only
    # tablet or phone) and it is being run on the wrong device, the test
    # just quits and does not do anything.  The java test harness will still
    # print the appropriate annotation for us, but will add --NORUN-- for
    # us so we know to ignore the results.
    # The --NORUN-- tag is managed by MainActivityTestBase.java
    if regex.group(1) != '--NORUN--':

      # Obtain the relevant perf data.  The data is dumped to a
      # JSON formatted file.
      json_string = self.adb.GetFileContents(
          '/data/data/com.google.android.apps.chrome/files/PerfTestData.txt')

      if json_string:
        json_string = '\n'.join(json_string)
      else:
        raise FatalTestException('Perf file does not exist or is empty')

      if self.save_perf_json:
        json_local_file = '/tmp/chromium-android-perf-json-' + raw_test_name
        with open(json_local_file, 'w') as f:
          f.write(json_string)
        logging.info('Saving Perf UI JSON from test ' +
                     test + ' to ' + json_local_file)

      raw_perf_data = regex.group(1).split(';')

      for raw_perf_set in raw_perf_data:
        if raw_perf_set:
          perf_set = raw_perf_set.split(',')
          if len(perf_set) != 3:
            raise FatalTestException('Unexpected number of tokens in '
                                     'perf annotation string: ' + raw_perf_set)

          # Process the performance data
          result = GetAverageRunInfoFromJSONString(json_string, perf_set[0])

          PrintPerfResult(perf_set[1], perf_set[2],
                          [result['average']], result['units'])

  def _SetupIndividualTestTimeoutScale(self, test):
    timeout_scale = self._GetIndividualTestTimeoutScale(test)
    valgrind_tools.SetChromeTimeoutScale(self.adb, timeout_scale)

  def _GetIndividualTestTimeoutScale(self, test):
    """Returns the timeout scale for the given |test|."""
    annotations = self.apks[0].GetTestAnnotations(test)
    timeout_scale = 1
    if 'TimeoutScale' in annotations:
      for annotation in annotations:
        scale_match = re.match('TimeoutScale:([0-9]+)', annotation)
        if scale_match:
          timeout_scale = int(scale_match.group(1))
    if self.wait_for_debugger:
      timeout_scale *= 100
    return timeout_scale

  def _GetIndividualTestTimeoutSecs(self, test):
    """Returns the timeout in seconds for the given |test|."""
    annotations = self.apks[0].GetTestAnnotations(test)
    if 'Manual' in annotations:
      return 600 * 60
    if 'External' in annotations:
      return 10 * 60
    if 'LargeTest' in annotations or _PERF_TEST_ANNOTATION in annotations:
      return 5 * 60
    if 'MediumTest' in annotations:
      return 3 * 60
    return 1 * 60

  def RunTests(self):
    """Runs the tests, generating the coverage if needed.

    Returns:
      A TestResults object.
    """
    instrumentation_path = (self.instrumentation_class_path +
                            '/android.test.InstrumentationTestRunner')
    instrumentation_args = self._GetInstrumentationArgs()
    for test in self._GetTestsIter():
      test_result = None
      start_date_ms = None
      try:
        self.TestSetup(test)
        start_date_ms = int(time.time()) * 1000
        args_with_filter = dict(instrumentation_args)
        args_with_filter['class'] = test
        # |test_results| is a list that should contain
        # a single TestResult object.
        logging.warn(args_with_filter)
        (test_results, _) = self.adb.Adb().StartInstrumentation(
            instrumentation_path=instrumentation_path,
            instrumentation_args=args_with_filter,
            timeout_time=(self._GetIndividualTestTimeoutSecs(test) *
                          self._GetIndividualTestTimeoutScale(test) *
                          self.tool.GetTimeoutScale()))
        duration_ms = int(time.time()) * 1000 - start_date_ms
        assert len(test_results) == 1
        test_result = test_results[0]
        status_code = test_result.GetStatusCode()
        if status_code:
          log = test_result.GetFailureReason()
          if not log:
            log = 'No information.'
          if self.screenshot_failures or log.find('INJECT_EVENTS perm') >= 0:
            self._TakeScreenshot(test)
          self.test_results.failed += [SingleTestResult(test, start_date_ms,
                                                        duration_ms, log)]
        else:
          result = [SingleTestResult(test, start_date_ms, duration_ms)]
          self.test_results.ok += result
      # Catch exceptions thrown by StartInstrumentation().
      # See ../../third_party/android/testrunner/adb_interface.py
      except (errors.WaitForResponseTimedOutError,
              errors.DeviceUnresponsiveError,
              errors.InstrumentationError), e:
        if start_date_ms:
          duration_ms = int(time.time()) * 1000 - start_date_ms
        else:
          start_date_ms = int(time.time()) * 1000
          duration_ms = 0
        message = str(e)
        if not message:
          message = 'No information.'
        self.test_results.crashed += [SingleTestResult(test, start_date_ms,
                                                       duration_ms,
                                                       message)]
        test_result = None
      self.TestTeardown(test, test_result)
    return self.test_results


class TestSharder(BaseTestSharder):
  """Responsible for sharding the tests on the connected devices."""

  def __init__(self, attached_devices, options, tests, apks):
    BaseTestSharder.__init__(self, attached_devices)
    self.options = options
    self.tests = tests
    self.apks = apks

  def SetupSharding(self, tests):
    """Called before starting the shards."""
    SetTestsContainer(sharded_tests_queue.ShardedTestsQueue(
        len(self.attached_devices), tests))

  def CreateShardedTestRunner(self, device, index):
    """Creates a sharded test runner.

    Args:
      device: Device serial where this shard will run.
      index: Index of this device in the pool.

    Returns:
      A TestRunner object.
    """
    return TestRunner(self.options, device, None, False, index, self.apks, [])


def DispatchJavaTests(options, apks):
  """Dispatches Java tests onto connected device(s).

  If possible, this method will attempt to shard the tests to
  all connected devices. Otherwise, dispatch and run tests on one device.

  Args:
    options: Command line options.
    apks: list of APKs to use.

  Returns:
    A TestResults object holding the results of the Java tests.

  Raises:
    FatalTestException: when there's no attached the devices.
  """
  test_apk = apks[0]
  if options.annotation:
    available_tests = test_apk.GetAnnotatedTests(options.annotation)
    if len(options.annotation) == 1 and options.annotation[0] == 'SmallTest':
      tests_without_annotation = [
          m for m in
          test_apk.GetTestMethods()
          if not test_apk.GetTestAnnotations(m) and
          not apk_info.ApkInfo.IsPythonDrivenTest(m)]
      if tests_without_annotation:
        tests_without_annotation.sort()
        logging.warning('The following tests do not contain any annotation. '
                        'Assuming "SmallTest":\n%s',
                        '\n'.join(tests_without_annotation))
        available_tests += tests_without_annotation
  else:
    available_tests = [m for m in test_apk.GetTestMethods()
                       if not apk_info.ApkInfo.IsPythonDrivenTest(m)]
  coverage = os.environ.get('EMMA_INSTRUMENT') == 'true'

  tests = []
  if options.test_filter:
    # |available_tests| are in adb instrument format: package.path.class#test.
    filter_without_hash = options.test_filter.replace('#', '.')
    tests = [t for t in available_tests
             if filter_without_hash in t.replace('#', '.')]
  else:
    tests = available_tests

  if not tests:
    logging.warning('No Java tests to run with current args.')
    return TestResults()

  tests *= options.number_of_runs

  attached_devices = android_commands.GetAttachedDevices()
  test_results = TestResults()

  if not attached_devices:
    raise FatalTestException('You have no devices attached or visible!')
  if options.device:
    attached_devices = [options.device]

  logging.info('Will run: %s', str(tests))

  if len(attached_devices) > 1 and (coverage or options.wait_for_debugger):
    logging.warning('Coverage / debugger can not be sharded, '
                    'using first available device')
    attached_devices = attached_devices[:1]
  sharder = TestSharder(attached_devices, options, tests, apks)
  test_results = sharder.RunShardedTests()
  return test_results

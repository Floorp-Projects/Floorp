# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import shlex
import sys
import tempfile
import time

import android_commands
import constants
from test_package import TestPackage
from pylib import pexpect

class TestPackageApk(TestPackage):
  """A helper class for running APK-based native tests.

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
               performance_test, cleanup_test_files, tool,
               dump_debug_info):
    TestPackage.__init__(self, adb, device, test_suite, timeout,
                         rebaseline, performance_test, cleanup_test_files,
                         tool, dump_debug_info)

  def _CreateTestRunnerScript(self, options):
    command_line_file = tempfile.NamedTemporaryFile()
    # GTest expects argv[0] to be the executable path.
    command_line_file.write(self.test_suite_basename + ' ' + options)
    command_line_file.flush()
    self.adb.PushIfNeeded(command_line_file.name,
                          constants.TEST_EXECUTABLE_DIR +
                          '/chrome-native-tests-command-line')

  def _GetGTestReturnCode(self):
    return None

  def _GetFifo(self):
    # The test.fifo path is determined by:
    # testing/android/java/src/org/chromium/native_test/
    #     ChromeNativeTestActivity.java and
    # testing/android/native_test_launcher.cc
    return '/data/data/org.chromium.native_test/files/test.fifo'

  def _ClearFifo(self):
    self.adb.RunShellCommand('rm -f ' + self._GetFifo())

  def _WatchFifo(self, timeout, logfile=None):
    for i in range(10):
      if self.adb.FileExistsOnDevice(self._GetFifo()):
        print 'Fifo created...'
        break
      time.sleep(i)
    else:
      raise Exception('Unable to find fifo on device %s ' % self._GetFifo())
    args = shlex.split(self.adb.Adb()._target_arg)
    args += ['shell', 'cat', self._GetFifo()]
    return pexpect.spawn('adb', args, timeout=timeout, logfile=logfile)

  def GetAllTests(self):
    """Returns a list of all tests available in the test suite."""
    self._CreateTestRunnerScript('--gtest_list_tests')
    try:
      self.tool.SetupEnvironment()
      # Clear and start monitoring logcat.
      self._ClearFifo()
      self.adb.RunShellCommand(
          'am start -n '
          'org.chromium.native_test/'
          'org.chromium.native_test.ChromeNativeTestActivity')
      # Wait for native test to complete.
      p = self._WatchFifo(timeout=30 * self.tool.GetTimeoutScale())
      p.expect("<<ScopedMainEntryLogger")
      p.close()
    finally:
      self.tool.CleanUpEnvironment()
    # We need to strip the trailing newline.
    content = [line.rstrip() for line in p.before.splitlines()]
    ret = self._ParseGTestListTests(content)
    return ret

  def CreateTestRunnerScript(self, gtest_filter, test_arguments):
    self._CreateTestRunnerScript('--gtest_filter=%s %s' % (gtest_filter,
                                                           test_arguments))

  def RunTestsAndListResults(self):
    try:
      self.tool.SetupEnvironment()
      self._ClearFifo()
      self.adb.RunShellCommand(
       'am start -n '
        'org.chromium.native_test/'
        'org.chromium.native_test.ChromeNativeTestActivity')
    finally:
      self.tool.CleanUpEnvironment()
    logfile = android_commands.NewLineNormalizer(sys.stdout)
    return self._WatchTestOutput(self._WatchFifo(timeout=10, logfile=logfile))

  def StripAndCopyExecutable(self):
    # Always uninstall the previous one (by activity name); we don't
    # know what was embedded in it.
    self.adb.ManagedInstall(self.test_suite_full, False,
                            package_name='org.chromium.native_test')

  def _GetTestSuiteBaseName(self):
    """Returns the  base name of the test suite."""
    # APK test suite names end with '-debug.apk'
    return os.path.basename(self.test_suite).rsplit('-debug', 1)[0]

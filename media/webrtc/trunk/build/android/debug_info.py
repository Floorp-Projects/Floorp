# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Collect debug info for a test."""

import datetime
import logging
import os
import re
import shutil
import string
import subprocess
import tempfile

import cmd_helper


TOMBSTONE_DIR = '/data/tombstones/'


class GTestDebugInfo(object):
  """A helper class to get relate debug information for a gtest.

  Args:
    adb: ADB interface the tests are using.
    device: Serial# of the Android device in which the specified gtest runs.
    testsuite_name: Name of the specified gtest.
    gtest_filter: Test filter used by the specified gtest.
  """

  def __init__(self, adb, device, testsuite_name, gtest_filter,
              collect_new_crashes=True):
    """Initializes the DebugInfo class for a specified gtest."""
    self.adb = adb
    self.device = device
    self.testsuite_name = testsuite_name
    self.gtest_filter = gtest_filter
    self.logcat_process = None
    self.has_storage = False
    self.log_dir = None
    self.log_file_name = None
    self.collect_new_crashes = collect_new_crashes
    self.old_crash_files = self.ListCrashFiles()

  def InitStorage(self):
    """Initializes the storage in where we put the debug information."""
    if self.has_storage:
      return
    self.has_storage = True
    self.log_dir = tempfile.mkdtemp()
    self.log_file_name = os.path.join(self.log_dir,
                                      self._GeneratePrefixName() + '_log.txt')

  def CleanupStorage(self):
    """Cleans up the storage in where we put the debug information."""
    if not self.has_storage:
      return
    self.has_storage = False
    assert os.path.exists(self.log_dir)
    shutil.rmtree(self.log_dir)
    self.log_dir = None
    self.log_file_name = None

  def GetStoragePath(self):
    """Returns the path in where we store the debug information."""
    self.InitStorage()
    return self.log_dir

  def _GetSignatureFromGTestFilter(self):
    """Gets a signature from gtest_filter.

    Signature is used to identify the tests from which we collect debug
    information.

    Returns:
      A signature string. Returns 'all' if there is no gtest filter.
    """
    if not self.gtest_filter:
      return 'all'
    filename_chars = "-_()%s%s" % (string.ascii_letters, string.digits)
    return ''.join(c for c in self.gtest_filter if c in filename_chars)

  def _GeneratePrefixName(self):
    """Generates a prefix name for debug information of the test.

    The prefix name consists of the following:
    (1) root name of test_suite_base.
    (2) device serial number.
    (3) filter signature generate from gtest_filter.
    (4) date & time when calling this method.

    Returns:
      Name of the log file.
    """
    return (os.path.splitext(self.testsuite_name)[0] + '_' + self.device + '_' +
            self._GetSignatureFromGTestFilter() + '_' +
            datetime.datetime.utcnow().strftime('%Y-%m-%d-%H-%M-%S-%f'))

  def StartRecordingLog(self, clear=True, filters=['*:v']):
    """Starts recording logcat output to a file.

    This call should come before running test, with calling StopRecordingLog
    following the tests.

    Args:
      clear: True if existing log output should be cleared.
      filters: A list of logcat filters to be used.
    """
    self.InitStorage()
    self.StopRecordingLog()
    if clear:
      cmd_helper.RunCmd(['adb', 'logcat', '-c'])
    logging.info('Start dumping log to %s ...' % self.log_file_name)
    command = 'adb logcat -v threadtime %s > %s' % (' '.join(filters),
                                                    self.log_file_name)
    self.logcat_process = subprocess.Popen(command, shell=True)

  def StopRecordingLog(self):
    """Stops an existing logcat recording subprocess."""
    if not self.logcat_process:
      return
    # Cannot evaluate directly as 0 is a possible value.
    if self.logcat_process.poll() is None:
      self.logcat_process.kill()
    self.logcat_process = None
    logging.info('Finish log dump.')

  def TakeScreenshot(self, identifier_mark):
    """Takes a screen shot from current specified device.

    Args:
      identifier_mark: A string to identify the screen shot DebugInfo will take.
                       It will be part of filename of the screen shot. Empty
                       string is acceptable.
    Returns:
      Returns True if successfully taking screen shot from device, otherwise
      returns False.
    """
    self.InitStorage()
    assert isinstance(identifier_mark, str)
    shot_path = os.path.join(self.log_dir, ''.join([self._GeneratePrefixName(),
                                                    identifier_mark,
                                                    '_screenshot.png']))
    screenshot_path = os.path.join(os.getenv('ANDROID_HOST_OUT'), 'bin',
                                   'screenshot2')
    re_success = re.compile(re.escape('Success.'), re.MULTILINE)
    if re_success.findall(cmd_helper.GetCmdOutput([screenshot_path, '-s',
                                                   self.device, shot_path])):
      logging.info("Successfully took a screen shot to %s" % shot_path)
      return True
    logging.error('Failed to take screen shot from device %s' % self.device)
    return False

  def ListCrashFiles(self):
    """Collects crash files from current specified device.

    Returns:
      A dict of crash files in format {"name": (size, lastmod), ...}.
    """
    if not self.collect_new_crashes:
      return {}
    return self.adb.ListPathContents(TOMBSTONE_DIR)

  def ArchiveNewCrashFiles(self):
    """Archives the crash files newly generated until calling this method."""
    if not self.collect_new_crashes:
      return
    current_crash_files = self.ListCrashFiles()
    files = [f for f in current_crash_files if f not in self.old_crash_files]
    logging.info('New crash file(s):%s' % ' '.join(files))
    for f in files:
      self.adb.Adb().Pull(TOMBSTONE_DIR + f,
                          os.path.join(self.GetStoragePath(), f))

  @staticmethod
  def ZipAndCleanResults(dest_dir, dump_file_name, debug_info_list):
    """A helper method to zip all debug information results into a dump file.

    Args:
      dest-dir: Dir path in where we put the dump file.
      dump_file_name: Desired name of the dump file. This method makes sure
                      '.zip' will be added as ext name.
      debug_info_list: List of all debug info objects.
    """
    if not dest_dir or not dump_file_name or not debug_info_list:
      return
    cmd_helper.RunCmd(['mkdir', '-p', dest_dir])
    log_basename = os.path.basename(dump_file_name)
    log_file = os.path.join(dest_dir,
                            os.path.splitext(log_basename)[0] + '.zip')
    logging.info('Zipping debug dumps into %s ...' % log_file)
    for d in debug_info_list:
      d.ArchiveNewCrashFiles()
    # Add new dumps into the zip file. The zip may exist already if previous
    # gtest also dumps the debug information. It's OK since we clean up the old
    # dumps in each build step.
    cmd_helper.RunCmd(['zip', '-q', '-r', log_file,
                       ' '.join([d.GetStoragePath() for d in debug_info_list])])
    assert os.path.exists(log_file)
    for debug_info in debug_info_list:
      debug_info.CleanupStorage()

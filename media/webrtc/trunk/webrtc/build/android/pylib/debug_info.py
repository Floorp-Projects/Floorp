# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  """A helper class to collect related debug information for a gtest.

  Debug info is collected in two steps:
  - first, object(s) of this class (one per device), accumulate logs
  and screenshots in tempdir.
  - once the test has finished, call ZipAndCleanResults to create
  a zip containing the logs from all devices, and clean them up.

  Args:
    adb: ADB interface the tests are using.
    device: Serial# of the Android device in which the specified gtest runs.
    testsuite_name: Name of the specified gtest.
    gtest_filter: Test filter used by the specified gtest.
  """

  def __init__(self, adb, device, testsuite_name, gtest_filter):
    """Initializes the DebugInfo class for a specified gtest."""
    self.adb = adb
    self.device = device
    self.testsuite_name = testsuite_name
    self.gtest_filter = gtest_filter
    self.logcat_process = None
    self.has_storage = False
    self.log_dir = os.path.join(tempfile.gettempdir(),
                                'gtest_debug_info',
                                self.testsuite_name,
                                self.device)
    if not os.path.exists(self.log_dir):
      os.makedirs(self.log_dir)
    self.log_file_name = os.path.join(self.log_dir,
                                      self._GeneratePrefixName() + '_log.txt')
    self.old_crash_files = self._ListCrashFiles()

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
    signature = ''.join(c for c in self.gtest_filter if c in filename_chars)
    if len(signature) > 64:
      # The signature can't be too long, as it'll be part of a file name.
      signature = signature[:64]
    return signature

  def _GeneratePrefixName(self):
    """Generates a prefix name for debug information of the test.

    The prefix name consists of the following:
    (1) root name of test_suite_base.
    (2) device serial number.
    (3) prefix of filter signature generate from gtest_filter.
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
    self.StopRecordingLog()
    if clear:
      cmd_helper.RunCmd(['adb', '-s', self.device, 'logcat', '-c'])
    logging.info('Start dumping log to %s ...', self.log_file_name)
    command = 'adb -s %s logcat -v threadtime %s > %s' % (self.device,
                                                          ' '.join(filters),
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
      Returns the file name on the host of the screenshot if successful,
      None otherwise.
    """
    assert isinstance(identifier_mark, str)
    screenshot_path = os.path.join(os.getenv('ANDROID_HOST_OUT', ''),
                                   'bin',
                                   'screenshot2')
    if not os.path.exists(screenshot_path):
      logging.error('Failed to take screen shot from device %s', self.device)
      return None
    shot_path = os.path.join(self.log_dir, ''.join([self._GeneratePrefixName(),
                                                    identifier_mark,
                                                    '_screenshot.png']))
    re_success = re.compile(re.escape('Success.'), re.MULTILINE)
    if re_success.findall(cmd_helper.GetCmdOutput([screenshot_path, '-s',
                                                   self.device, shot_path])):
      logging.info('Successfully took a screen shot to %s', shot_path)
      return shot_path
    logging.error('Failed to take screen shot from device %s', self.device)
    return None

  def _ListCrashFiles(self):
    """Collects crash files from current specified device.

    Returns:
      A dict of crash files in format {"name": (size, lastmod), ...}.
    """
    return self.adb.ListPathContents(TOMBSTONE_DIR)

  def ArchiveNewCrashFiles(self):
    """Archives the crash files newly generated until calling this method."""
    current_crash_files = self._ListCrashFiles()
    files = []
    for f in current_crash_files:
      if f not in self.old_crash_files:
        files += [f]
      elif current_crash_files[f] != self.old_crash_files[f]:
        # Tombstones dir can only have maximum 10 files, so we need to compare
        # size and timestamp information of file if the file exists.
        files += [f]
    if files:
      logging.info('New crash file(s):%s' % ' '.join(files))
      for f in files:
        self.adb.Adb().Pull(TOMBSTONE_DIR + f,
                            os.path.join(self.log_dir, f))

  @staticmethod
  def ZipAndCleanResults(dest_dir, dump_file_name):
    """A helper method to zip all debug information results into a dump file.

    Args:
      dest_dir: Dir path in where we put the dump file.
      dump_file_name: Desired name of the dump file. This method makes sure
                      '.zip' will be added as ext name.
    """
    if not dest_dir or not dump_file_name:
      return
    cmd_helper.RunCmd(['mkdir', '-p', dest_dir])
    log_basename = os.path.basename(dump_file_name)
    log_zip_file = os.path.join(dest_dir,
                                os.path.splitext(log_basename)[0] + '.zip')
    logging.info('Zipping debug dumps into %s ...', log_zip_file)
    # Add new dumps into the zip file. The zip may exist already if previous
    # gtest also dumps the debug information. It's OK since we clean up the old
    # dumps in each build step.
    log_src_dir = os.path.join(tempfile.gettempdir(), 'gtest_debug_info')
    cmd_helper.RunCmd(['zip', '-q', '-r', log_zip_file, log_src_dir])
    assert os.path.exists(log_zip_file)
    assert os.path.exists(log_src_dir)
    shutil.rmtree(log_src_dir)

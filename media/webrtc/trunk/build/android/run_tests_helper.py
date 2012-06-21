# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper functions common to native test runners."""

import logging
import optparse
import os
import subprocess
import sys

# TODO(michaelbai): Move constant definitions like below to a common file.
FORWARDER_PATH = '/data/local/tmp/forwarder'

CHROME_DIR = os.path.abspath(os.path.join(sys.path[0], '..', '..'))


def IsRunningAsBuildbot():
  """Returns True if we are currently running on buildbot; False otherwise."""
  return bool(os.getenv('BUILDBOT_BUILDERNAME'))


def ReportBuildbotLink(label, url):
  """Adds a link with name |label| linking to |url| to current buildbot step.

  Args:
    label: A string with the name of the label.
    url: A string of the URL.
  """
  if IsRunningAsBuildbot():
    print '@@@STEP_LINK@%s@%s@@@' % (label, url)


def ReportBuildbotMsg(msg):
  """Appends |msg| to the current buildbot step text.

  Args:
    msg: String to be appended.
  """
  if IsRunningAsBuildbot():
    print '@@@STEP_TEXT@%s@@@' % msg

def ReportBuildbotError():
  """Marks the current step as failed."""
  if IsRunningAsBuildbot():
    print '@@@STEP_FAILURE@@@'


def GetExpectations(file_name):
  """Returns a list of test names in the |file_name| test expectations file."""
  if not file_name or not os.path.exists(file_name):
    return []
  return [x for x in [x.strip() for x in file(file_name).readlines()]
          if x and x[0] != '#']


def SetLogLevel(verbose_count):
  """Sets log level as |verbose_count|."""
  log_level = logging.WARNING  # Default.
  if verbose_count == 1:
    log_level = logging.INFO
  elif verbose_count >= 2:
    log_level = logging.DEBUG
  logging.getLogger().setLevel(log_level)


def CreateTestRunnerOptionParser(usage=None, default_timeout=60):
  """Returns a new OptionParser with arguments applicable to all tests."""
  option_parser = optparse.OptionParser(usage=usage)
  option_parser.add_option('-t', dest='timeout',
                           help='Timeout to wait for each test',
                           type='int',
                           default=default_timeout)
  option_parser.add_option('-c', dest='cleanup_test_files',
                           help='Cleanup test files on the device after run',
                           action='store_true',
                           default=False)
  option_parser.add_option('-v',
                           '--verbose',
                           dest='verbose_count',
                           default=0,
                           action='count',
                           help='Verbose level (multiple times for more)')
  option_parser.add_option('--tool',
                           dest='tool',
                           help='Run the test under a tool '
                           '(use --tool help to list them)')
  return option_parser


def ForwardDevicePorts(adb, ports, host_name='127.0.0.1'):
  """Forwards a TCP port on the device back to the host.

  Works like adb forward, but in reverse.

  Args:
    adb: Instance of AndroidCommands for talking to the device.
    ports: A list of tuples (device_port, host_port) to forward.
    host_name: Optional. Address to forward to, must be addressable from the
               host machine. Usually this is omitted and loopback is used.

  Returns:
    subprocess instance connected to the forwarder process on the device.
  """
  adb.PushIfNeeded(
      os.path.join(CHROME_DIR, 'out', 'Release', 'forwarder'), FORWARDER_PATH)
  forward_string = ['%d:%d:%s' %
                    (device, host, host_name) for device, host in ports]
  logging.info("Forwarding ports: %s" % (forward_string))

  return subprocess.Popen(
      ['adb', '-s', adb._adb.GetSerialNumber(),
       'shell', '%s -D %s' % (FORWARDER_PATH, ' '.join(forward_string))])


def IsDevicePortUsed(adb, device_port):
  """Checks whether the specified device port is used or not.

  Args:
    adb: Instance of AndroidCommands for talking to the device.
    device_port: Port on device we want to check.

  Returns:
    True if the port on device is already used, otherwise returns False.
  """
  base_url = '127.0.0.1:%d' % device_port
  netstat_results = adb.RunShellCommand('netstat')
  for single_connect in netstat_results:
    # Column 3 is the local address which we want to check with.
    if single_connect.split()[3] == base_url:
      return True
  return False

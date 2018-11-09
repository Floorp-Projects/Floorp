# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import android_commands
import constants
import logging
import os
import subprocess
import time


class FakeDns(object):
  """Wrapper class for the fake_dns tool."""
  _FAKE_DNS_PATH = constants.TEST_EXECUTABLE_DIR + '/fake_dns'

  def __init__(self, adb, build_type):
    """
      Args:
        adb: the AndroidCommands to use.
        build_type: 'Release' or 'Debug'.
    """
    self._adb = adb
    self._build_type = build_type
    self._fake_dns = None
    self._original_dns = None

  def _PushAndStartFakeDns(self):
    """Starts the fake_dns server that replies all name queries 127.0.0.1.

    Returns:
      subprocess instance connected to the fake_dns process on the device.
    """
    self._adb.PushIfNeeded(
        os.path.join(constants.CHROME_DIR, 'out', self._build_type, 'fake_dns'),
        FakeDns._FAKE_DNS_PATH)
    return subprocess.Popen(
        ['adb', '-s', self._adb._adb.GetSerialNumber(),
         'shell', '%s -D' % FakeDns._FAKE_DNS_PATH])

  def SetUp(self):
    """Configures the system to point to a DNS server that replies 127.0.0.1.

    This can be used in combination with the forwarder to forward all web
    traffic to a replay server.

    The TearDown() method will perform all cleanup.
    """
    self._adb.RunShellCommand('ip route add 8.8.8.0/24 via 127.0.0.1 dev lo')
    self._fake_dns = self._PushAndStartFakeDns()
    self._original_dns = self._adb.RunShellCommand('getprop net.dns1')[0]
    self._adb.RunShellCommand('setprop net.dns1 127.0.0.1')
    time.sleep(2)  # Time for server to start and the setprop to take effect.

  def TearDown(self):
    """Shuts down the fake_dns."""
    if self._fake_dns:
      if not self._original_dns or self._original_dns == '127.0.0.1':
        logging.warning('Bad original DNS, falling back to Google DNS.')
        self._original_dns = '8.8.8.8'
      self._adb.RunShellCommand('setprop net.dns1 %s' % self._original_dns)
      self._fake_dns.kill()
      self._adb.RunShellCommand('ip route del 8.8.8.0/24 via 127.0.0.1 dev lo')

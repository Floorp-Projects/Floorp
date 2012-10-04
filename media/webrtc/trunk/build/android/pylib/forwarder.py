# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import pexpect
import re
import sys

import android_commands
from constants import CHROME_DIR

class Forwarder(object):
  """Class to manage port forwards from the device to the host."""

  _FORWARDER_PATH = '/data/local/tmp/forwarder'
  _TIMEOUT_SECS = 30

  def __init__(self, adb, port_pairs, tool, host_name, build_type):
    """Forwards TCP ports on the device back to the host.

    Works like adb forward, but in reverse.

    Args:
      adb: Instance of AndroidCommands for talking to the device.
      port_pairs: A list of tuples (device_port, host_port) to forward. Note
                 that you can specify 0 as a device_port, in which case a
                 port will by dynamically assigned on the device. You can
                 get the number of the assigned port using the
                 DevicePortForHostPort method.
      tool: Tool class to use to get wrapper, if necessary, for executing the
            forwarder (see valgrind_tools.py).
      host_name: Address to forward to, must be addressable from the
                 host machine. Usually use loopback '127.0.0.1'.
      build_type: 'Release' or 'Debug'.

    Raises:
      Exception on failure to forward the port.
    """
    self._adb = adb
    self._host_to_device_port_map = dict()
    self._process = None
    adb.PushIfNeeded(
        os.path.join(CHROME_DIR, 'out', build_type, 'forwarder'),
        Forwarder._FORWARDER_PATH)
    forward_string = ['%d:%d:%s' %
                      (device, host, host_name) for device, host in port_pairs]

    # Kill off any existing forwarders on conflicting non-dynamically allocated
    # ports.
    for device_port, _ in port_pairs:
      if device_port != 0:
        self._KillForwardersUsingDevicePort(device_port)

    logging.info('Forwarding ports: %s' % (forward_string))
    process = pexpect.spawn(
      'adb', ['-s', adb._adb.GetSerialNumber(),
              'shell', '%s %s -D %s' % (
          tool.GetUtilWrapper(), Forwarder._FORWARDER_PATH,
          ' '.join(forward_string))])

    # Read the output of the command to determine which device ports where
    # forwarded to which host ports (necessary if
    success_re = re.compile('Forwarding device port (\d+) to host (\d+):')
    failure_re = re.compile('Couldn\'t start forwarder server for port spec: '
                            '(\d+):(\d+)')
    for pair in port_pairs:
      index = process.expect([success_re, failure_re, pexpect.EOF,
                              pexpect.TIMEOUT],
                             Forwarder._TIMEOUT_SECS)
      if index == 0:
        # Success
        device_port = int(process.match.group(1))
        host_port = int(process.match.group(2))
        self._host_to_device_port_map[host_port] = device_port
        logging.info("Forwarding device port: %d to host port: %d." %
                     (device_port, host_port))
      elif index == 1:
        # Failure
        device_port = int(process.match.group(1))
        host_port = int(process.match.group(2))
        process.close()
        raise Exception('Failed to forward port %d to %d' % (device_port,
                                                             host_port))
      elif index == 2:
        logging.error(process.before)
        process.close()
        raise Exception('Unexpected EOF while trying to forward ports %s' %
                        port_pairs)
      elif index == 3:
        logging.error(process.before)
        process.close()
        raise Exception('Timeout while trying to forward ports %s' % port_pairs)

    self._process = process

  def _KillForwardersUsingDevicePort(self, device_port):
    """Check if the device port is in use and if it is try to terminate the
       forwarder process (if any) that may already be forwarding it"""
    processes = self._adb.ProcessesUsingDevicePort(device_port)
    for pid, name in processes:
      if name == 'forwarder':
        logging.warning(
            'Killing forwarder process with pid %d using device_port %d' % (
                 pid, device_port))
        self._adb.RunShellCommand('kill %d' % pid)
      else:
        logging.error(
             'Not killing process with pid %d (%s) using device_port %d' % (
                 pid, name, device_port))

  def DevicePortForHostPort(self, host_port):
    """Get the device port that corresponds to a given host port."""
    return self._host_to_device_port_map.get(host_port)

  def Close(self):
    """Terminate the forwarder process."""
    if self._process:
      self._process.close()
      self._process = None

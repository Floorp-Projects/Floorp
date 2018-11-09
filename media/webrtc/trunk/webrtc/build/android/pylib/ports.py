# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions that deals with local and device ports."""

import contextlib
import fcntl
import httplib
import logging
import os
import re
import socket
import traceback

import cmd_helper
import constants


#The following two methods are used to allocate the port source for various
# types of test servers. Because some net relates tests can be run on shards
# at same time, it's important to have a mechanism to allocate the port process
# safe. In here, we implement the safe port allocation by leveraging flock.
def ResetTestServerPortAllocation():
  """Reset the port allocation to start from TEST_SERVER_PORT_FIRST.

  Returns:
    Returns True if reset successes. Otherwise returns False.
  """
  try:
    with open(constants.TEST_SERVER_PORT_FILE, 'w') as fp:
      fp.write('%d' % constants.TEST_SERVER_PORT_FIRST)
    if os.path.exists(constants.TEST_SERVER_PORT_LOCKFILE):
      os.unlink(constants.TEST_SERVER_PORT_LOCKFILE)
    return True
  except Exception as e:
    logging.error(e)
  return False


def AllocateTestServerPort():
  """Allocate a port incrementally.

  Returns:
    Returns a valid port which should be in between TEST_SERVER_PORT_FIRST and
    TEST_SERVER_PORT_LAST. Returning 0 means no more valid port can be used.
  """
  port = 0
  ports_tried = []
  try:
    fp_lock = open(constants.TEST_SERVER_PORT_LOCKFILE, 'w')
    fcntl.flock(fp_lock, fcntl.LOCK_EX)
    # Get current valid port and calculate next valid port.
    assert os.path.exists(constants.TEST_SERVER_PORT_FILE)
    with open(constants.TEST_SERVER_PORT_FILE, 'r+') as fp:
      port = int(fp.read())
      ports_tried.append(port)
      while IsHostPortUsed(port):
        port += 1
        ports_tried.append(port)
      if (port > constants.TEST_SERVER_PORT_LAST or
          port < constants.TEST_SERVER_PORT_FIRST):
        port = 0
      else:
        fp.seek(0, os.SEEK_SET)
        fp.write('%d' % (port + 1))
  except Exception as e:
    logging.info(e)
  finally:
    if fp_lock:
      fcntl.flock(fp_lock, fcntl.LOCK_UN)
      fp_lock.close()
  if port:
    logging.info('Allocate port %d for test server.', port)
  else:
    logging.error('Could not allocate port for test server. '
                  'List of ports tried: %s', str(ports_tried))
  return port


def IsHostPortUsed(host_port):
  """Checks whether the specified host port is used or not.

  Uses -n -P to inhibit the conversion of host/port numbers to host/port names.

  Args:
    host_port: Port on host we want to check.

  Returns:
    True if the port on host is already used, otherwise returns False.
  """
  port_info = '(127\.0\.0\.1)|(localhost)\:%d' % host_port
  # TODO(jnd): Find a better way to filter the port.
  re_port = re.compile(port_info, re.MULTILINE)
  if re_port.findall(cmd_helper.GetCmdOutput(['lsof', '-nPi:%d' % host_port])):
    return True
  return False


def IsDevicePortUsed(adb, device_port, state=''):
  """Checks whether the specified device port is used or not.

  Args:
    adb: Instance of AndroidCommands for talking to the device.
    device_port: Port on device we want to check.
    state: String of the specified state. Default is empty string, which
           means any state.

  Returns:
    True if the port on device is already used, otherwise returns False.
  """
  base_url = '127.0.0.1:%d' % device_port
  netstat_results = adb.RunShellCommand('netstat', log_result=False)
  for single_connect in netstat_results:
    # Column 3 is the local address which we want to check with.
    connect_results = single_connect.split()
    is_state_match = connect_results[5] == state if state else True
    if connect_results[3] == base_url and is_state_match:
      return True
  return False


def IsHttpServerConnectable(host, port, tries=3, command='GET', path='/',
                            expected_read='', timeout=2):
  """Checks whether the specified http server is ready to serve request or not.

  Args:
    host: Host name of the HTTP server.
    port: Port number of the HTTP server.
    tries: How many times we want to test the connection. The default value is
           3.
    command: The http command we use to connect to HTTP server. The default
             command is 'GET'.
    path: The path we use when connecting to HTTP server. The default path is
          '/'.
    expected_read: The content we expect to read from the response. The default
                   value is ''.
    timeout: Timeout (in seconds) for each http connection. The default is 2s.

  Returns:
    Tuple of (connect status, client error). connect status is a boolean value
    to indicate whether the server is connectable. client_error is the error
    message the server returns when connect status is false.
  """
  assert tries >= 1
  for i in xrange(0, tries):
    client_error = None
    try:
      with contextlib.closing(httplib.HTTPConnection(
          host, port, timeout=timeout)) as http:
        # Output some debug information when we have tried more than 2 times.
        http.set_debuglevel(i >= 2)
        http.request(command, path)
        r = http.getresponse()
        content = r.read()
        if r.status == 200 and r.reason == 'OK' and content == expected_read:
          return (True, '')
        client_error = ('Bad response: %s %s version %s\n  ' %
                        (r.status, r.reason, r.version) +
                        '\n  '.join([': '.join(h) for h in r.getheaders()]))
    except (httplib.HTTPException, socket.error) as e:
      # Probably too quick connecting: try again.
      exception_error_msgs = traceback.format_exception_only(type(e), e)
      if exception_error_msgs:
        client_error = ''.join(exception_error_msgs)
  # Only returns last client_error.
  return (False, client_error or 'Timeout')

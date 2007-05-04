#!c:/Python24/python.exe
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is standalone Firefox Windows performance test.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Annie Sullivan <annie.sullivan@gmail.com> (original author)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

"""A set of functions for process management on Windows.
"""

__author__ = 'annie.sullivan@gmail.com (Annie Sullivan)'


import msvcrt
import os
import re
import time
import win32api
import win32file
import win32pdhutil
import win32pipe

import paths


def GetCygwinPath(dos_path):
  """Helper function to get the Cygwin path from a dos path.
     Used to generate a Firefox command line piped through the
     Cygwin bash shell

  Args:
    dos_path: String containing the dos path

  Returns:
    String containing the cygwin path
  """

  # Convert the path to a cygwin path. Assumes the path starts with
  # /cygdrive/driveletter
  cygwin_path = '/' + dos_path[3:]                       # Remove 'C:\'
  cygwin_path = cygwin_path.replace('\\', '/')           # Backslashes->slashes
  cygwin_path = cygwin_path.replace(' ', '\\ ')          # Escape spaces
  cygwin_path = '/cygdrive/' + dos_path[0] + cygwin_path # Add drive letter
  return cygwin_path


def GenerateFirefoxCommandLine(firefox_path, profile_dir, url):
  """Generates the command line for a process to run Firefox, wrapped
     by cygwin so that we can read the output from dump() statements.

  Args:
    firefox_path: String containing the path to the firefox exe to use
    profile_dir: String containing the directory of the profile to run Firefox in
    url: String containing url to start with.
  """

  profile_arg = ''
  if profile_dir:
    profile_dir = profile_dir.replace('\\', '\\\\\\')
    profile_arg = '-profile %s' % profile_dir

  url_arg = ''
  if url:
    url_arg = '-url %s' % url

  cmd = '%s "%s %s %s"' % (paths.CYGWIN,
                           GetCygwinPath(firefox_path),
                           profile_arg,
                           url_arg)
  return cmd


def SyncAndSleep():
  """Runs sync and sleeps for a few seconds between Firefox runs.
     Otherwise "Firefox is already running.." errors occur
  """

  os.spawnl(os.P_WAIT, paths.SYNC)
  time.sleep(3)


def TerminateProcess(pid):
  """Helper function to terminate a process, given the pid

  Args:
    pid: integer process id of the process to terminate.
  """

  PROCESS_TERMINATE = 1
  handle = win32api.OpenProcess(PROCESS_TERMINATE, False, pid)
  win32api.TerminateProcess(handle, -1)
  win32api.CloseHandle(handle)


def ProcessesWithNameExist(process_name):
  """Returns true if there are any processes running with the
     given name.  Useful to check whether a Firefox process is still running

  Args:
    process_name: String containing the process name, i.e. "firefox"

  Returns:
    True if any processes with that name are running, False otherwise.
  """

  try:
    pids = win32pdhutil.FindPerformanceAttributesByName(process_name, counter="ID Process")
    return len(pids) > 0
  except:
    # Might get an exception if there are no instances of the process running.
    return False


def TerminateAllProcesses(process_name):
  """Helper function to terminate all processes with the given process name

  Args:
    process_name: String containing the process name, i.e. "firefox"
  """

  # Get all the process ids of running instances of this process, and terminate them.
  try:
    pids = win32pdhutil.FindPerformanceAttributesByName(process_name, counter="ID Process")
    for pid in pids:
      TerminateProcess(pid)
  except:
    # Might get an exception if there are no instances of the process running.
    pass


def NonBlockingReadProcessOutput(handle):
  """Does a non-blocking read from the output of the process
     with the given handle.

  Args:
    handle: The process handle returned from os.popen()

  Returns:
    A tuple (bytes, output) containing the number of output
    bytes read, and the actual output.
  """

  output = ""

  try:
    osfhandle = msvcrt.get_osfhandle(handle.fileno())
    (read, num_avail, num_message) = win32pipe.PeekNamedPipe(osfhandle, 0)
    if num_avail > 0:
      (error_code, output) = win32file.ReadFile(osfhandle, num_avail, None)

    return (num_avail, output)
  except:
    return (0, output)


def RunProcessAndWaitForOutput(command, process_name, output_regex, timeout):
  """Runs the given process and waits for the output that matches the given
     regular expression.  Stops if the process exits early or times out.

  Args:
    command: String containing command to run
    process_name: Name of the process to run, in case it has to be killed
    output_regex: Regular expression to check against each output line.
                  If the output matches, the process is terminated and 
                  the function returns.
    timeout: Time to wait before terminating the process and returning

  Returns:
    A tuple (match, timedout) where match is the match of the regular 
    expression, and timed out is true if the process timed out and 
    false otherwise.
  """

  # Start the process
  handle = os.popen(command)

  # Wait for it to print output, terminate, or time out.
  time_elapsed = 0
  output = ''
  interval = 2 # Wait 2 seconds in between checks

  while time_elapsed < timeout:
    time.sleep(interval)
    time_elapsed += interval

    (bytes, current_output) = NonBlockingReadProcessOutput(handle)
    output += current_output

    result = output_regex.search(output)
    if result:
      try:
        return_val = result.group(1)
        TerminateAllProcesses(process_name)
        return (return_val, False)
      except IndexError:
        # Didn't really match
        pass

  # Timed out.
  TerminateAllProcesses(process_name)
  return (None, True)

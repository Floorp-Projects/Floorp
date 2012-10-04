# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides an interface to communicate with the device via the adb command.

Assumes adb binary is currently on system path.
"""

import collections
import datetime
import logging
import os
import random
import re
import shlex
import subprocess
import sys
import tempfile
import time

import pexpect
import io_stats_parser

# adb_interface.py is under ../../../third_party/android_testrunner/
sys.path.append(os.path.join(os.path.abspath(os.path.dirname(__file__)), '..',
                             '..', '..', 'third_party', 'android_testrunner'))
import adb_interface
import cmd_helper
import errors  #  is under ../../../third_party/android_testrunner/errors.py


# Pattern to search for the next whole line of pexpect output and capture it
# into a match group. We can't use ^ and $ for line start end with pexpect,
# see http://www.noah.org/python/pexpect/#doc for explanation why.
PEXPECT_LINE_RE = re.compile('\n([^\r]*)\r')

# Set the adb shell prompt to be a unique marker that will [hopefully] not
# appear at the start of any line of a command's output.
SHELL_PROMPT = '~+~PQ\x17RS~+~'

# This only works for single core devices.
SCALING_GOVERNOR = '/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor'
DROP_CACHES = '/proc/sys/vm/drop_caches'

# Java properties file
LOCAL_PROPERTIES_PATH = '/data/local.prop'

# Property in /data/local.prop that controls Java assertions.
JAVA_ASSERT_PROPERTY = 'dalvik.vm.enableassertions'

BOOT_COMPLETE_RE = re.compile(
    'android.intent.action.MEDIA_MOUNTED path: /\w+/sdcard\d?'
    '|PowerManagerService(\(\s+\d+\))?: bootCompleted')

MEMORY_INFO_RE = re.compile('^(?P<key>\w+):\s+(?P<usage_kb>\d+) kB$')
NVIDIA_MEMORY_INFO_RE = re.compile('^\s*(?P<user>\S+)\s*(?P<name>\S+)\s*'
                                   '(?P<pid>\d+)\s*(?P<usage_bytes>\d+)$')

# Keycode "enum" suitable for passing to AndroidCommands.SendKey().
KEYCODE_HOME = 3
KEYCODE_BACK = 4
KEYCODE_DPAD_UP = 19
KEYCODE_DPAD_DOWN = 20
KEYCODE_DPAD_RIGHT = 22
KEYCODE_ENTER = 66
KEYCODE_MENU = 82


def GetEmulators():
  """Returns a list of emulators.  Does not filter by status (e.g. offline).

  Both devices starting with 'emulator' will be returned in below output:

    * daemon not running. starting it now on port 5037 *
    * daemon started successfully *
    List of devices attached
    027c10494100b4d7        device
    emulator-5554   offline
    emulator-5558   device
  """
  re_device = re.compile('^emulator-[0-9]+', re.MULTILINE)
  devices = re_device.findall(cmd_helper.GetCmdOutput(['adb', 'devices']))
  return devices


def GetAVDs():
  """Returns a list of AVDs."""
  re_avd = re.compile('^[ ]+Name: ([a-zA-Z0-9_:.-]+)', re.MULTILINE)
  avds = re_avd.findall(cmd_helper.GetCmdOutput(['android', 'list', 'avd']))
  return avds


def GetAttachedDevices():
  """Returns a list of attached, online android devices.

  If a preferred device has been set with ANDROID_SERIAL, it will be first in
  the returned list.

  Example output:

    * daemon not running. starting it now on port 5037 *
    * daemon started successfully *
    List of devices attached
    027c10494100b4d7        device
    emulator-5554   offline
  """
  re_device = re.compile('^([a-zA-Z0-9_:.-]+)\tdevice$', re.MULTILINE)
  devices = re_device.findall(cmd_helper.GetCmdOutput(['adb', 'devices']))
  preferred_device = os.environ.get('ANDROID_SERIAL')
  if preferred_device in devices:
    devices.remove(preferred_device)
    devices.insert(0, preferred_device)
  return devices


def _GetHostFileInfo(file_name):
  """Returns a tuple containing size and modified UTC time for file_name."""
  # The time accuracy on device is only to minute level, remove the second and
  # microsecond from host results.
  utc_time = datetime.datetime.utcfromtimestamp(os.path.getmtime(file_name))
  time_delta = datetime.timedelta(seconds=utc_time.second,
                                  microseconds=utc_time.microsecond)
  return os.path.getsize(file_name), utc_time - time_delta


def ListHostPathContents(path):
  """Lists files in all subdirectories of |path|.

  Args:
    path: The path to list.

  Returns:
    A dict of {"name": (size, lastmod), ...}.
  """
  if os.path.isfile(path):
    return {os.path.basename(path): _GetHostFileInfo(path)}
  ret = {}
  for root, dirs, files in os.walk(path):
    for d in dirs:
      if d.startswith('.'):
        dirs.remove(d)  # Prune the dir for subsequent iterations.
    for f in files:
      if f.startswith('.'):
        continue
      full_file_name = os.path.join(root, f)
      file_name = os.path.relpath(full_file_name, path)
      ret[file_name] = _GetHostFileInfo(full_file_name)
  return ret


def _GetFilesFromRecursiveLsOutput(path, ls_output, re_file, utc_offset=None):
  """Gets a list of files from `ls` command output.

  Python's os.walk isn't used because it doesn't work over adb shell.

  Args:
    path: The path to list.
    ls_output: A list of lines returned by an `ls -lR` command.
    re_file: A compiled regular expression which parses a line into named groups
        consisting of at minimum "filename", "date", "time", "size" and
        optionally "timezone".
    utc_offset: A 5-character string of the form +HHMM or -HHMM, where HH is a
        2-digit string giving the number of UTC offset hours, and MM is a
        2-digit string giving the number of UTC offset minutes. If the input
        utc_offset is None, will try to look for the value of "timezone" if it
        is specified in re_file.

  Returns:
    A dict of {"name": (size, lastmod), ...} where:
      name: The file name relative to |path|'s directory.
      size: The file size in bytes (0 for directories).
      lastmod: The file last modification date in UTC.
  """
  re_directory = re.compile('^%s/(?P<dir>[^:]+):$' % re.escape(path))
  path_dir = os.path.dirname(path)

  current_dir = ''
  files = {}
  for line in ls_output:
    directory_match = re_directory.match(line)
    if directory_match:
      current_dir = directory_match.group('dir')
      continue
    file_match = re_file.match(line)
    if file_match:
      filename = os.path.join(current_dir, file_match.group('filename'))
      if filename.startswith(path_dir):
        filename = filename[len(path_dir)+1:]
      lastmod = datetime.datetime.strptime(
          file_match.group('date') + ' ' + file_match.group('time')[:5],
          '%Y-%m-%d %H:%M')
      if not utc_offset and 'timezone' in re_file.groupindex:
        utc_offset = file_match.group('timezone')
      if isinstance(utc_offset, str) and len(utc_offset) == 5:
        utc_delta = datetime.timedelta(hours=int(utc_offset[1:3]),
                                       minutes=int(utc_offset[3:5]))
        if utc_offset[0:1] == '-':
          utc_delta = -utc_delta
        lastmod -= utc_delta
      files[filename] = (int(file_match.group('size')), lastmod)
  return files


def GetLogTimestamp(log_line, year):
  """Returns the timestamp of the given |log_line| in the given year."""
  try:
    return datetime.datetime.strptime('%s-%s' % (year, log_line[:18]),
                                      '%Y-%m-%d %H:%M:%S.%f')
  except (ValueError, IndexError):
    logging.critical('Error reading timestamp from ' + log_line)
    return None


class AndroidCommands(object):
  """Helper class for communicating with Android device via adb.

  Args:
    device: If given, adb commands are only send to the device of this ID.
        Otherwise commands are sent to all attached devices.
  """

  def __init__(self, device=None):
    self._adb = adb_interface.AdbInterface()
    if device:
      self._adb.SetTargetSerial(device)
    self._logcat = None
    self._original_governor = None
    self._pushed_files = []
    self._device_utc_offset = self.RunShellCommand('date +%z')[0]

  def Adb(self):
    """Returns our AdbInterface to avoid us wrapping all its methods."""
    return self._adb

  def IsRootEnabled(self):
    """Checks if root is enabled on the device."""
    root_test_output = self.RunShellCommand('ls /root') or ['']
    return not 'Permission denied' in root_test_output[0]

  def GetDeviceYear(self):
    """Returns the year information of the date on device."""
    return self.RunShellCommand('date +%Y')[0]

  def WaitForDevicePm(self):
    """Blocks until the device's package manager is available.

    To workaround http://b/5201039, we restart the shell and retry if the
    package manager isn't back after 120 seconds.

    Raises:
      errors.WaitForResponseTimedOutError after max retries reached.
    """
    last_err = None
    retries = 3
    while retries:
      try:
        self._adb.WaitForDevicePm()
        return  # Success
      except errors.WaitForResponseTimedOutError as e:
        last_err = e
        logging.warning('Restarting and retrying after timeout: %s', e)
        retries -= 1
        self.RestartShell()
    raise last_err  # Only reached after max retries, re-raise the last error.

  def SynchronizeDateTime(self):
    """Synchronize date/time between host and device."""
    self._adb.SendShellCommand('date -u %f' % time.time())

  def RestartShell(self):
    """Restarts the shell on the device. Does not block for it to return."""
    self.RunShellCommand('stop')
    self.RunShellCommand('start')

  def Reboot(self, full_reboot=True):
    """Reboots the device and waits for the package manager to return.

    Args:
      full_reboot: Whether to fully reboot the device or just restart the shell.
    """
    # TODO(torne): hive can't reboot the device either way without breaking the
    # connection; work out if we can handle this better
    if os.environ.get('USING_HIVE'):
      logging.warning('Ignoring reboot request as we are on hive')
      return
    if full_reboot or not self.IsRootEnabled():
      self._adb.SendCommand('reboot')
      timeout = 300
    else:
      self.RestartShell()
      timeout = 120
    self.WaitForDevicePm()
    self.StartMonitoringLogcat(timeout=timeout)
    self.WaitForLogMatch(BOOT_COMPLETE_RE, None)

  def Uninstall(self, package):
    """Uninstalls the specified package from the device.

    Args:
      package: Name of the package to remove.

    Returns:
      A status string returned by adb uninstall
    """
    uninstall_command = 'uninstall %s' % package

    logging.info('>>> $' + uninstall_command)
    return self._adb.SendCommand(uninstall_command, timeout_time=60)

  def Install(self, package_file_path, reinstall=False):
    """Installs the specified package to the device.

    Args:
      package_file_path: Path to .apk file to install.
      reinstall: Reinstall an existing apk, keeping the data.

    Returns:
      A status string returned by adb install
    """
    assert os.path.isfile(package_file_path)

    install_cmd = ['install']

    if reinstall:
      install_cmd.append('-r')

    install_cmd.append(package_file_path)
    install_cmd = ' '.join(install_cmd)

    logging.info('>>> $' + install_cmd)
    return self._adb.SendCommand(install_cmd, timeout_time=2*60, retry_count=0)

  def ManagedInstall(self, apk_path, keep_data=False, package_name=None,
                     reboots_on_failure=2):
    """Installs specified package and reboots device on timeouts.

    Args:
      apk_path: Path to .apk file to install.
      keep_data: Reinstalls instead of uninstalling first, preserving the
        application data.
      package_name: Package name (only needed if keep_data=False).
      reboots_on_failure: number of time to reboot if package manager is frozen.

    Returns:
      A status string returned by adb install
    """
    reboots_left = reboots_on_failure
    while True:
      try:
        if not keep_data:
          assert package_name
          self.Uninstall(package_name)
        install_status = self.Install(apk_path, reinstall=keep_data)
        if 'Success' in install_status:
          return install_status
      except errors.WaitForResponseTimedOutError:
        print '@@@STEP_WARNINGS@@@'
        logging.info('Timeout on installing %s' % apk_path)

      if reboots_left <= 0:
        raise Exception('Install failure')

      # Force a hard reboot on last attempt
      self.Reboot(full_reboot=(reboots_left == 1))
      reboots_left -= 1

  def MakeSystemFolderWritable(self):
    """Remounts the /system folder rw."""
    out = self._adb.SendCommand('remount')
    if out.strip() != 'remount succeeded':
      raise errors.MsgException('Remount failed: %s' % out)

  def RestartAdbServer(self):
    """Restart the adb server."""
    self.KillAdbServer()
    self.StartAdbServer()

  def KillAdbServer(self):
    """Kill adb server."""
    adb_cmd = ['adb', 'kill-server']
    return cmd_helper.RunCmd(adb_cmd)

  def StartAdbServer(self):
    """Start adb server."""
    adb_cmd = ['adb', 'start-server']
    return cmd_helper.RunCmd(adb_cmd)

  def WaitForSystemBootCompleted(self, wait_time):
    """Waits for targeted system's boot_completed flag to be set.

    Args:
      wait_time: time in seconds to wait

    Raises:
      WaitForResponseTimedOutError if wait_time elapses and flag still not
      set.
    """
    logging.info('Waiting for system boot completed...')
    self._adb.SendCommand('wait-for-device')
    # Now the device is there, but system not boot completed.
    # Query the sys.boot_completed flag with a basic command
    boot_completed = False
    attempts = 0
    wait_period = 5
    while not boot_completed and (attempts * wait_period) < wait_time:
      output = self._adb.SendShellCommand('getprop sys.boot_completed',
                                          retry_count=1)
      output = output.strip()
      if output == '1':
        boot_completed = True
      else:
        # If 'error: xxx' returned when querying the flag, it means
        # adb server lost the connection to the emulator, so restart the adb
        # server.
        if 'error:' in output:
          self.RestartAdbServer()
        time.sleep(wait_period)
        attempts += 1
    if not boot_completed:
      raise errors.WaitForResponseTimedOutError(
          'sys.boot_completed flag was not set after %s seconds' % wait_time)

  def WaitForSdCardReady(self, timeout_time):
    """Wait for the SD card ready before pushing data into it."""
    logging.info('Waiting for SD card ready...')
    sdcard_ready = False
    attempts = 0
    wait_period = 5
    while not sdcard_ready and attempts * wait_period < timeout_time:
      output = self.RunShellCommand('ls /sdcard/')
      if output:
        sdcard_ready = True
      else:
        time.sleep(wait_period)
        attempts += 1
    if not sdcard_ready:
      raise errors.WaitForResponseTimedOutError(
          'SD card not ready after %s seconds' % timeout_time)

  # It is tempting to turn this function into a generator, however this is not
  # possible without using a private (local) adb_shell instance (to ensure no
  # other command interleaves usage of it), which would defeat the main aim of
  # being able to reuse the adb shell instance across commands.
  def RunShellCommand(self, command, timeout_time=20, log_result=False):
    """Send a command to the adb shell and return the result.

    Args:
      command: String containing the shell command to send. Must not include
               the single quotes as we use them to escape the whole command.
      timeout_time: Number of seconds to wait for command to respond before
        retrying, used by AdbInterface.SendShellCommand.
      log_result: Boolean to indicate whether we should log the result of the
                  shell command.

    Returns:
      list containing the lines of output received from running the command
    """
    logging.info('>>> $' + command)
    if "'" in command: logging.warning(command + " contains ' quotes")
    result = self._adb.SendShellCommand(
        "'%s'" % command, timeout_time).splitlines()
    if ['error: device not found'] == result:
      raise errors.DeviceUnresponsiveError('device not found')
    if log_result:
      logging.info('\n>>> '.join(result))
    return result

  def KillAll(self, process):
    """Android version of killall, connected via adb.

    Args:
      process: name of the process to kill off

    Returns:
      the number of processess killed
    """
    pids = self.ExtractPid(process)
    if pids:
      self.RunShellCommand('kill ' + ' '.join(pids))
    return len(pids)

  def StartActivity(self, package, activity, wait_for_completion=False,
                    action='android.intent.action.VIEW',
                    category=None, data=None,
                    extras=None, trace_file_name=None):
    """Starts |package|'s activity on the device.

    Args:
      package: Name of package to start (e.g. 'com.google.android.apps.chrome').
      activity: Name of activity (e.g. '.Main' or
        'com.google.android.apps.chrome.Main').
      wait_for_completion: wait for the activity to finish launching (-W flag).
      action: string (e.g. "android.intent.action.MAIN"). Default is VIEW.
      category: string (e.g. "android.intent.category.HOME")
      data: Data string to pass to activity (e.g. 'http://www.example.com/').
      extras: Dict of extras to pass to activity. Values are significant.
      trace_file_name: If used, turns on and saves the trace to this file name.
    """
    cmd = 'am start -a %s' % action
    if wait_for_completion:
      cmd += ' -W'
    if category:
      cmd += ' -c %s' % category
    if package and activity:
      cmd += ' -n %s/%s' % (package, activity)
    if data:
      cmd += ' -d "%s"' % data
    if extras:
      for key in extras:
        value = extras[key]
        if isinstance(value, str):
          cmd += ' --es'
        elif isinstance(value, bool):
          cmd += ' --ez'
        elif isinstance(value, int):
          cmd += ' --ei'
        else:
          raise NotImplementedError(
              'Need to teach StartActivity how to pass %s extras' % type(value))
        cmd += ' %s %s' % (key, value)
    if trace_file_name:
      cmd += ' --start-profiler ' + trace_file_name
    self.RunShellCommand(cmd)

  def CloseApplication(self, package):
    """Attempt to close down the application, using increasing violence.

    Args:
      package: Name of the process to kill off, e.g.
      com.google.android.apps.chrome
    """
    self.RunShellCommand('am force-stop ' + package)

  def ClearApplicationState(self, package):
    """Closes and clears all state for the given |package|."""
    self.CloseApplication(package)
    self.RunShellCommand('rm -r /data/data/%s/app_*' % package)
    self.RunShellCommand('rm -r /data/data/%s/cache/*' % package)
    self.RunShellCommand('rm -r /data/data/%s/files/*' % package)
    self.RunShellCommand('rm -r /data/data/%s/shared_prefs/*' % package)

  def SendKeyEvent(self, keycode):
    """Sends keycode to the device.

    Args:
      keycode: Numeric keycode to send (see "enum" at top of file).
    """
    self.RunShellCommand('input keyevent %d' % keycode)

  def PushIfNeeded(self, local_path, device_path):
    """Pushes |local_path| to |device_path|.

    Works for files and directories. This method skips copying any paths in
    |test_data_paths| that already exist on the device with the same timestamp
    and size.

    All pushed files can be removed by calling RemovePushedFiles().
    """
    assert os.path.exists(local_path), 'Local path not found %s' % local_path
    self._pushed_files.append(device_path)

    # If the path contents are the same, there's nothing to do.
    local_contents = ListHostPathContents(local_path)
    device_contents = self.ListPathContents(device_path)
    # Only compare the size and timestamp if only copying a file because
    # the filename on device can be renamed.
    if os.path.isfile(local_path):
      assert len(local_contents) == 1
      is_equal = local_contents.values() == device_contents.values()
    else:
      is_equal = local_contents == device_contents
    if is_equal:
      logging.info('%s is up-to-date. Skipping file push.', device_path)
      return

    # They don't match, so remove everything first and then create it.
    if os.path.isdir(local_path):
      self.RunShellCommand('rm -r %s' % device_path, timeout_time=2*60)
      self.RunShellCommand('mkdir -p %s' % device_path)

    # NOTE: We can't use adb_interface.Push() because it hardcodes a timeout of
    # 60 seconds which isn't sufficient for a lot of users of this method.
    push_command = 'push %s %s' % (local_path, device_path)
    logging.info('>>> $' + push_command)
    output = self._adb.SendCommand(push_command, timeout_time=30*60)
    assert output
    # Success looks like this: "3035 KB/s (12512056 bytes in 4.025s)"
    # Errors look like this: "failed to copy  ... "
    if not re.search('^[0-9]', output.splitlines()[-1]):
      logging.critical('PUSH FAILED: ' + output)

  def GetFileContents(self, filename, log_result=False):
    """Gets contents from the file specified by |filename|."""
    return self.RunShellCommand('if [ -f "' + filename + '" ]; then cat "' +
                                filename + '"; fi', log_result=log_result)

  def SetFileContents(self, filename, contents):
    """Writes |contents| to the file specified by |filename|."""
    with tempfile.NamedTemporaryFile() as f:
      f.write(contents)
      f.flush()
      self._adb.Push(f.name, filename)

  def RemovePushedFiles(self):
    """Removes all files pushed with PushIfNeeded() from the device."""
    for p in self._pushed_files:
      self.RunShellCommand('rm -r %s' % p, timeout_time=2*60)

  def ListPathContents(self, path):
    """Lists files in all subdirectories of |path|.

    Args:
      path: The path to list.

    Returns:
      A dict of {"name": (size, lastmod), ...}.
    """
    # Example output:
    # /foo/bar:
    # -rw-r----- 1 user group   102 2011-05-12 12:29:54.131623387 +0100 baz.txt
    re_file = re.compile('^-(?P<perms>[^\s]+)\s+'
                         '(?P<user>[^\s]+)\s+'
                         '(?P<group>[^\s]+)\s+'
                         '(?P<size>[^\s]+)\s+'
                         '(?P<date>[^\s]+)\s+'
                         '(?P<time>[^\s]+)\s+'
                         '(?P<filename>[^\s]+)$')
    return _GetFilesFromRecursiveLsOutput(
        path, self.RunShellCommand('ls -lR %s' % path), re_file,
        self._device_utc_offset)

  def SetupPerformanceTest(self):
    """Sets up performance tests."""
    # Disable CPU scaling to reduce noise in tests
    if not self._original_governor:
      self._original_governor = self.GetFileContents(
          SCALING_GOVERNOR, log_result=False)
      self.RunShellCommand('echo performance > ' + SCALING_GOVERNOR)
    self.DropRamCaches()

  def TearDownPerformanceTest(self):
    """Tears down performance tests."""
    if self._original_governor:
      self.RunShellCommand('echo %s > %s' % (self._original_governor[0],
                                             SCALING_GOVERNOR))
    self._original_governor = None

  def SetJavaAssertsEnabled(self, enable):
    """Sets or removes the device java assertions property.

    Args:
      enable: If True the property will be set.

    Returns:
      True if the file was modified (reboot is required for it to take effect).
    """
    # First ensure the desired property is persisted.
    temp_props_file = tempfile.NamedTemporaryFile()
    properties = ''
    if self._adb.Pull(LOCAL_PROPERTIES_PATH, temp_props_file.name):
      properties = file(temp_props_file.name).read()
    re_search = re.compile(r'^\s*' + re.escape(JAVA_ASSERT_PROPERTY) +
                           r'\s*=\s*all\s*$', re.MULTILINE)
    if enable != bool(re.search(re_search, properties)):
      re_replace = re.compile(r'^\s*' + re.escape(JAVA_ASSERT_PROPERTY) +
                              r'\s*=\s*\w+\s*$', re.MULTILINE)
      properties = re.sub(re_replace, '', properties)
      if enable:
        properties += '\n%s=all\n' % JAVA_ASSERT_PROPERTY

      file(temp_props_file.name, 'w').write(properties)
      self._adb.Push(temp_props_file.name, LOCAL_PROPERTIES_PATH)

    # Next, check the current runtime value is what we need, and
    # if not, set it and report that a reboot is required.
    was_set = 'all' in self.RunShellCommand('getprop ' + JAVA_ASSERT_PROPERTY)
    if was_set == enable:
      return False

    self.RunShellCommand('setprop %s "%s"' % (JAVA_ASSERT_PROPERTY,
                                              enable and 'all' or ''))
    return True

  def DropRamCaches(self):
    """Drops the filesystem ram caches for performance testing."""
    self.RunShellCommand('echo 3 > ' + DROP_CACHES)

  def StartMonitoringLogcat(self, clear=True, timeout=10, logfile=None,
                            filters=None):
    """Starts monitoring the output of logcat, for use with WaitForLogMatch.

    Args:
      clear: If True the existing logcat output will be cleared, to avoiding
             matching historical output lurking in the log.
      timeout: How long WaitForLogMatch will wait for the given match
      filters: A list of logcat filters to be used.
    """
    if clear:
      self.RunShellCommand('logcat -c')
    args = []
    if self._adb._target_arg:
      args += shlex.split(self._adb._target_arg)
    args += ['logcat', '-v', 'threadtime']
    if filters:
      args.extend(filters)
    else:
      args.append('*:v')

    # Spawn logcat and syncronize with it.
    for _ in range(4):
      self._logcat = pexpect.spawn('adb', args, timeout=timeout,
                                   logfile=logfile)
      self.RunShellCommand('log startup_sync')
      if self._logcat.expect(['startup_sync', pexpect.EOF,
                              pexpect.TIMEOUT]) == 0:
        break
      self._logcat.close(force=True)
    else:
      logging.critical('Error reading from logcat: ' + str(self._logcat.match))
      sys.exit(1)

  def GetMonitoredLogCat(self):
    """Returns an "adb logcat" command as created by pexpected.spawn."""
    if not self._logcat:
      self.StartMonitoringLogcat(clear=False)
    return self._logcat

  def WaitForLogMatch(self, success_re, error_re, clear=False):
    """Blocks until a matching line is logged or a timeout occurs.

    Args:
      success_re: A compiled re to search each line for.
      error_re: A compiled re which, if found, terminates the search for
          |success_re|. If None is given, no error condition will be detected.
      clear: If True the existing logcat output will be cleared, defaults to
          false.

    Raises:
      pexpect.TIMEOUT upon the timeout specified by StartMonitoringLogcat().

    Returns:
      The re match object if |success_re| is matched first or None if |error_re|
      is matched first.
    """
    if not self._logcat:
      self.StartMonitoringLogcat(clear)
    logging.info('<<< Waiting for logcat:' + str(success_re.pattern))
    t0 = time.time()
    try:
      while True:
        # Note this will block for upto the timeout _per log line_, so we need
        # to calculate the overall timeout remaining since t0.
        time_remaining = t0 + self._logcat.timeout - time.time()
        if time_remaining < 0: raise pexpect.TIMEOUT(self._logcat)
        self._logcat.expect(PEXPECT_LINE_RE, timeout=time_remaining)
        line = self._logcat.match.group(1)
        if error_re:
          error_match = error_re.search(line)
          if error_match:
            return None
        success_match = success_re.search(line)
        if success_match:
          return success_match
        logging.info('<<< Skipped Logcat Line:' + str(line))
    except pexpect.TIMEOUT:
      raise pexpect.TIMEOUT(
          'Timeout (%ds) exceeded waiting for pattern "%s" (tip: use -vv '
          'to debug)' %
          (self._logcat.timeout, success_re.pattern))

  def StartRecordingLogcat(self, clear=True, filters=['*:v']):
    """Starts recording logcat output to eventually be saved as a string.

    This call should come before some series of tests are run, with either
    StopRecordingLogcat or SearchLogcatRecord following the tests.

    Args:
      clear: True if existing log output should be cleared.
      filters: A list of logcat filters to be used.
    """
    if clear:
      self._adb.SendCommand('logcat -c')
    logcat_command = 'adb %s logcat -v threadtime %s' % (self._adb._target_arg,
                                                         ' '.join(filters))
    self.logcat_process = subprocess.Popen(logcat_command, shell=True,
                                           stdout=subprocess.PIPE)

  def StopRecordingLogcat(self):
    """Stops an existing logcat recording subprocess and returns output.

    Returns:
      The logcat output as a string or an empty string if logcat was not
      being recorded at the time.
    """
    if not self.logcat_process:
      return ''
    # Cannot evaluate directly as 0 is a possible value.
    # Better to read the self.logcat_process.stdout before killing it,
    # Otherwise the communicate may return incomplete output due to pipe break.
    if self.logcat_process.poll() is None:
      self.logcat_process.kill()
    (output, _) = self.logcat_process.communicate()
    self.logcat_process = None
    return output

  def SearchLogcatRecord(self, record, message, thread_id=None, proc_id=None,
                         log_level=None, component=None):
    """Searches the specified logcat output and returns results.

    This method searches through the logcat output specified by record for a
    certain message, narrowing results by matching them against any other
    specified criteria.  It returns all matching lines as described below.

    Args:
      record: A string generated by Start/StopRecordingLogcat to search.
      message: An output string to search for.
      thread_id: The thread id that is the origin of the message.
      proc_id: The process that is the origin of the message.
      log_level: The log level of the message.
      component: The name of the component that would create the message.

    Returns:
      A list of dictionaries represeting matching entries, each containing keys
      thread_id, proc_id, log_level, component, and message.
    """
    if thread_id:
      thread_id = str(thread_id)
    if proc_id:
      proc_id = str(proc_id)
    results = []
    reg = re.compile('(\d+)\s+(\d+)\s+([A-Z])\s+([A-Za-z]+)\s*:(.*)$',
                     re.MULTILINE)
    log_list = reg.findall(record)
    for (tid, pid, log_lev, comp, msg) in log_list:
      if ((not thread_id or thread_id == tid) and
          (not proc_id or proc_id == pid) and
          (not log_level or log_level == log_lev) and
          (not component or component == comp) and msg.find(message) > -1):
        match = dict({'thread_id': tid, 'proc_id': pid,
                      'log_level': log_lev, 'component': comp,
                      'message': msg})
        results.append(match)
    return results

  def ExtractPid(self, process_name):
    """Extracts Process Ids for a given process name from Android Shell.

    Args:
      process_name: name of the process on the device.

    Returns:
      List of all the process ids (as strings) that match the given name.
      If the name of a process exactly matches the given name, the pid of
      that process will be inserted to the front of the pid list.
    """
    pids = []
    for line in self.RunShellCommand('ps', log_result=False):
      data = line.split()
      try:
        if process_name in data[-1]:  # name is in the last column
          if process_name == data[-1]:
            pids.insert(0, data[1])  # PID is in the second column
          else:
            pids.append(data[1])
      except IndexError:
        pass
    return pids

  def GetIoStats(self):
    """Gets cumulative disk IO stats since boot (for all processes).

    Returns:
      Dict of {num_reads, num_writes, read_ms, write_ms} or None if there
      was an error.
    """
    for line in self.GetFileContents('/proc/diskstats', log_result=False):
      stats = io_stats_parser.ParseIoStatsLine(line)
      if stats.device == 'mmcblk0':
        return {
            'num_reads': stats.num_reads_issued,
            'num_writes': stats.num_writes_completed,
            'read_ms': stats.ms_spent_reading,
            'write_ms': stats.ms_spent_writing,
        }
    logging.warning('Could not find disk IO stats.')
    return None

  def GetMemoryUsageForPid(self, pid):
    """Returns the memory usage for given pid.

    Args:
      pid: The pid number of the specific process running on device.

    Returns:
      A tuple containg:
      [0]: Dict of {metric:usage_kb}, for the process which has specified pid.
      The metric keys which may be included are: Size, Rss, Pss, Shared_Clean,
      Shared_Dirty, Private_Clean, Private_Dirty, Referenced, Swap,
      KernelPageSize, MMUPageSize, Nvidia (tablet only).
      [1]: Detailed /proc/[PID]/smaps information.
    """
    usage_dict = collections.defaultdict(int)
    smaps = collections.defaultdict(dict)
    current_smap = ''
    for line in self.GetFileContents('/proc/%s/smaps' % pid, log_result=False):
      items = line.split()
      # See man 5 proc for more details. The format is:
      # address perms offset dev inode pathname
      if len(items) > 5:
        current_smap = ' '.join(items[5:])
      elif len(items) > 3:
        current_smap = ' '.join(items[3:])
      match = re.match(MEMORY_INFO_RE, line)
      if match:
        key = match.group('key')
        usage_kb = int(match.group('usage_kb'))
        usage_dict[key] += usage_kb
        if key not in smaps[current_smap]:
          smaps[current_smap][key] = 0
        smaps[current_smap][key] += usage_kb
    if not usage_dict or not any(usage_dict.values()):
      # Presumably the process died between ps and calling this method.
      logging.warning('Could not find memory usage for pid ' + str(pid))

    for line in self.GetFileContents('/d/nvmap/generic-0/clients',
                                     log_result=False):
      match = re.match(NVIDIA_MEMORY_INFO_RE, line)
      if match and match.group('pid') == pid:
        usage_bytes = int(match.group('usage_bytes'))
        usage_dict['Nvidia'] = int(round(usage_bytes / 1000.0))  # kB
        break

    return (usage_dict, smaps)

  def GetMemoryUsageForPackage(self, package):
    """Returns the memory usage for all processes whose name contains |pacakge|.

    Args:
      package: A string holding process name to lookup pid list for.

    Returns:
      A tuple containg:
      [0]: Dict of {metric:usage_kb}, summed over all pids associated with
           |name|.
      The metric keys which may be included are: Size, Rss, Pss, Shared_Clean,
      Shared_Dirty, Private_Clean, Private_Dirty, Referenced, Swap,
      KernelPageSize, MMUPageSize, Nvidia (tablet only).
      [1]: a list with detailed /proc/[PID]/smaps information.
    """
    usage_dict = collections.defaultdict(int)
    pid_list = self.ExtractPid(package)
    smaps = collections.defaultdict(dict)

    for pid in pid_list:
      usage_dict_per_pid, smaps_per_pid = self.GetMemoryUsageForPid(pid)
      smaps[pid] = smaps_per_pid
      for (key, value) in usage_dict_per_pid.items():
        usage_dict[key] += value

    return usage_dict, smaps

  def ProcessesUsingDevicePort(self, device_port):
    """Lists processes using the specified device port on loopback interface.

    Args:
      device_port: Port on device we want to check.

    Returns:
      A list of (pid, process_name) tuples using the specified port.
    """
    tcp_results = self.RunShellCommand('cat /proc/net/tcp', log_result=False)
    tcp_address = '0100007F:%04X' % device_port
    pids = []
    for single_connect in tcp_results:
      connect_results = single_connect.split()
      # Column 1 is the TCP port, and Column 9 is the inode of the socket
      if connect_results[1] == tcp_address:
        socket_inode = connect_results[9]
        socket_name = 'socket:[%s]' % socket_inode
        lsof_results = self.RunShellCommand('lsof', log_result=False)
        for single_process in lsof_results:
          process_results = single_process.split()
          # Ignore the line if it has less than nine columns in it, which may
          # be the case when a process stops while lsof is executing.
          if len(process_results) <= 8:
            continue
          # Column 0 is the executable name
          # Column 1 is the pid
          # Column 8 is the Inode in use
          if process_results[8] == socket_name:
            pids.append((int(process_results[1]), process_results[0]))
        break
    logging.info('PidsUsingDevicePort: %s', pids)
    return pids

  def RunMonkey(self, package_name, category=None, throttle=100, seed=None,
                event_count=10000, verbosity=1, extra_args=''):
    """Runs monkey test for a given package.

    Args:
      package_name: Allowed package.
      category: A list of allowed categories.
      throttle: Delay between events (ms).
      seed: Seed value for pseduo-random generator. Same seed value
        generates the same sequence of events. Seed is randomized by
        default.
      event_count: Number of events to generate.
      verbosity: Verbosity level [0-3].
      extra_args: A string of other args to pass to the command verbatim.

    Returns:
      Output of the test run.
    """
    category = category or []
    seed = seed or random.randint(1, 100)

    cmd = ['monkey',
           '-p %s' % package_name,
           ' '.join(['-c %s' % c for c in category]),
           '--throttle %d' % throttle,
           '-s %d' % seed,
           '-v ' * verbosity,
           extra_args,
           '%s' % event_count]
    return self.RunShellCommand(' '.join(cmd),
                                timeout_time=event_count*throttle*1.5)

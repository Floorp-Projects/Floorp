#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to keep track of devices across builds and report state."""

import optparse
import os
import sys

from pylib.android_commands import GetAttachedDevices
from pylib.cmd_helper import GetCmdOutput


def DeviceInfo(serial):
  """Gathers info on a device via various adb calls.

  Args:
    serial: The serial of the attached device to construct info about.

  Returns:
    Tuple of device type, build id and report as a string.
  """

  def AdbShellCmd(cmd):
    return GetCmdOutput('adb -s %s shell %s' % (serial, cmd),
                        shell=True).strip()

  device_type = AdbShellCmd('getprop ro.build.product')
  device_build = AdbShellCmd('getprop ro.build.id')

  report = ['Device %s (%s)' % (serial, device_type),
            '  Build: %s (%s)' % (device_build,
                                  AdbShellCmd('getprop ro.build.fingerprint')),
            '  Battery: %s%%' % AdbShellCmd('dumpsys battery | grep level '
                                            "| awk '{print $2}'"),
            '  Battery temp: %s' % AdbShellCmd('dumpsys battery'
                                               '| grep temp '
                                               "| awk '{print $2}'"),
            '  IMEI slice: %s' % AdbShellCmd('dumpsys iphonesubinfo '
                                             '| grep Device'
                                             "| awk '{print $4}'")[-6:],
            '  Wifi IP: %s' % AdbShellCmd('getprop dhcp.wlan0.ipaddress'),
            '']

  return device_type, device_build, '\n'.join(report)


def CheckForMissingDevices(options, adb_online_devs):
  """Uses file of previous online devices to detect broken phones.

  Args:
    options: out_dir parameter of options argument is used as the base
             directory to load and update the cache file.
    adb_online_devs: A list of serial numbers of the currently visible
                     and online attached devices.
  """

  last_devices_path = os.path.abspath(os.path.join(options.out_dir,
                                                   '.last_devices'))
  last_devices = []
  try:
    with open(last_devices_path) as f:
      last_devices = f.read().splitlines()
  except IOError:
    # Ignore error, file might not exist
    pass

  missing_devs = list(set(last_devices) - set(adb_online_devs))
  if missing_devs:
    print '@@@STEP_WARNINGS@@@'
    print '@@@STEP_SUMMARY_TEXT@%s not detected.@@@' % missing_devs
    print 'Current online devices: %s' % adb_online_devs
    print '%s are no longer visible. Were they removed?\n' % missing_devs
    print 'SHERIFF: See go/chrome_device_monitor'
    print 'Cache file: %s\n\n' % last_devices_path
    print 'adb devices'
    print GetCmdOutput(['adb', 'devices'])
  else:
    new_devs = set(adb_online_devs) - set(last_devices)
    if new_devs:
      print '@@@STEP_WARNINGS@@@'
      print '@@@STEP_SUMMARY_TEXT@New devices detected :-)@@@'
      print ('New devices detected %s. And now back to your '
             'regularly scheduled program.' % list(new_devs))

  # Write devices currently visible plus devices previously seen.
  with open(last_devices_path, 'w') as f:
    f.write('\n'.join(set(adb_online_devs + last_devices)))


def main():
  parser = optparse.OptionParser()
  parser.add_option('', '--out-dir',
                    help='Directory where the device path is stored',
                    default=os.path.join(os.path.dirname(__file__), '..',
                                         '..', 'out'))

  options, args = parser.parse_args()
  if args:
    parser.error('Unknown options %s' % args)
  devices = GetAttachedDevices()

  types, builds, reports = [], [], []
  if devices:
    types, builds, reports = zip(*[DeviceInfo(dev) for dev in devices])

  unique_types = list(set(types))
  unique_builds = list(set(builds))

  print ('@@@BUILD_STEP Device Status Check - '
         '%d online devices, types %s, builds %s@@@'
         % (len(devices), unique_types, unique_builds))
  print '\n'.join(reports)
  CheckForMissingDevices(options, devices)

if __name__ == '__main__':
  sys.exit(main())

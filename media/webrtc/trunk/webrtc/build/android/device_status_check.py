#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to keep track of devices across builds and report state."""
import logging
import optparse
import os
import smtplib
import sys

from pylib import buildbot_report
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
  # TODO(navabi): remove this once the bug that causes different number
  # of devices to be detected between calls is fixed.
  logger = logging.getLogger()
  logger.setLevel(logging.INFO)

  out_dir = os.path.abspath(options.out_dir)

  def ReadDeviceList(file_name):
    devices_path = os.path.join(out_dir, file_name)
    devices = []
    try:
      with open(devices_path) as f:
        devices = f.read().splitlines()
    except IOError:
      # Ignore error, file might not exist
      pass
    return devices

  def WriteDeviceList(file_name, device_list):
    path = os.path.join(out_dir, file_name)
    if not os.path.exists(out_dir):
      os.makedirs(out_dir)
    with open(path, 'w') as f:
      # Write devices currently visible plus devices previously seen.
      f.write('\n'.join(set(device_list)))

  last_devices_path = os.path.join(out_dir, '.last_devices')
  last_devices = ReadDeviceList('.last_devices')

  missing_devs = list(set(last_devices) - set(adb_online_devs))
  if missing_devs:
    from_address = 'buildbot@chromium.org'
    to_address = 'chromium-android-device-alerts@google.com'
    bot_name = os.environ['BUILDBOT_BUILDERNAME']
    slave_name = os.environ['BUILDBOT_SLAVENAME']
    num_online_devs = len(adb_online_devs)
    subject = 'Devices offline on %s, %s (%d remaining).' % (slave_name,
                                                             bot_name,
                                                             num_online_devs)
    buildbot_report.PrintWarning()
    devices_missing_msg = '%d devices not detected.' % len(missing_devs)
    buildbot_report.PrintSummaryText(devices_missing_msg)

    # TODO(navabi): Debug by printing both output from GetCmdOutput and
    # GetAttachedDevices to compare results.
    body = '\n'.join(
        ['Current online devices: %s' % adb_online_devs,
         '%s are no longer visible. Were they removed?\n' % missing_devs,
         'SHERIFF: See go/chrome_device_monitor',
         'Cache file: %s\n\n' % last_devices_path,
         'adb devices: %s' % GetCmdOutput(['adb', 'devices']),
         'adb devices(GetAttachedDevices): %s' % GetAttachedDevices()])

    print body

    # Only send email if the first time a particular device goes offline
    last_missing = ReadDeviceList('.last_missing')
    new_missing_devs = set(missing_devs) - set(last_missing)

    if new_missing_devs:
      msg_body = '\r\n'.join(
          ['From: %s' % from_address,
           'To: %s' % to_address,
           'Subject: %s' % subject,
           '', body])
      try:
        server = smtplib.SMTP('localhost')
        server.sendmail(from_address, [to_address], msg_body)
        server.quit()
      except Exception as e:
        print 'Failed to send alert email. Error: %s' % e
  else:
    new_devs = set(adb_online_devs) - set(last_devices)
    if new_devs and os.path.exists(last_devices_path):
      buildbot_report.PrintWarning()
      buildbot_report.PrintSummaryText(
          '%d new devices detected' % len(new_devs))
      print ('New devices detected %s. And now back to your '
             'regularly scheduled program.' % list(new_devs))
  WriteDeviceList('.last_devices', (adb_online_devs + last_devices))
  WriteDeviceList('.last_missing', missing_devs)


def main():
  parser = optparse.OptionParser()
  parser.add_option('', '--out-dir',
                    help='Directory where the device path is stored',
                    default=os.path.join(os.path.dirname(__file__), '..',
                                         '..', 'out'))

  options, args = parser.parse_args()
  if args:
    parser.error('Unknown options %s' % args)
  buildbot_report.PrintNamedStep('Device Status Check')
  devices = GetAttachedDevices()
  types, builds, reports = [], [], []
  if devices:
    types, builds, reports = zip(*[DeviceInfo(dev) for dev in devices])

  unique_types = list(set(types))
  unique_builds = list(set(builds))

  buildbot_report.PrintMsg('Online devices: %d. Device types %s, builds %s'
                           % (len(devices), unique_types, unique_builds))
  print '\n'.join(reports)
  CheckForMissingDevices(options, devices)

if __name__ == '__main__':
  sys.exit(main())

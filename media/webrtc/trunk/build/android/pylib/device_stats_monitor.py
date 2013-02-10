# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for iotop/top style profiling for android."""

import collections
import json
import os
import subprocess
import sys
import urllib

import constants
import io_stats_parser


class DeviceStatsMonitor(object):
  """Class for collecting device stats such as IO/CPU usage.

  Args:
      adb: Instance of AndroidComannds.
      hz: Frequency at which to sample device stats.
  """

  DEVICE_PATH = constants.TEST_EXECUTABLE_DIR + '/device_stats_monitor'
  PROFILE_PATH = (constants.DEVICE_PERF_OUTPUT_DIR +
      '/device_stats_monitor.profile')
  RESULT_VIEWER_PATH = os.path.abspath(os.path.join(
      os.path.dirname(os.path.realpath(__file__)), 'device_stats_monitor.html'))

  def __init__(self, adb, hz, build_type):
    self._adb = adb
    host_path = os.path.abspath(os.path.join(
        constants.CHROME_DIR, 'out', build_type, 'device_stats_monitor'))
    self._adb.PushIfNeeded(host_path, DeviceStatsMonitor.DEVICE_PATH)
    self._hz = hz

  def Start(self):
    """Starts device stats monitor on the device."""
    self._adb.SetFileContents(DeviceStatsMonitor.PROFILE_PATH, '')
    self._process = subprocess.Popen(
        ['adb', 'shell', '%s --hz=%d %s' % (
            DeviceStatsMonitor.DEVICE_PATH, self._hz,
            DeviceStatsMonitor.PROFILE_PATH)])

  def StopAndCollect(self, output_path):
    """Stops monitoring and saves results.

    Args:
      output_path: Path to save results.

    Returns:
      String of URL to load results in browser.
    """
    assert self._process
    self._adb.KillAll(DeviceStatsMonitor.DEVICE_PATH)
    self._process.wait()
    profile = self._adb.GetFileContents(DeviceStatsMonitor.PROFILE_PATH)

    results = collections.defaultdict(list)
    last_io_stats = None
    last_cpu_stats = None
    for line in profile:
      if ' mmcblk0 ' in line:
        stats = io_stats_parser.ParseIoStatsLine(line)
        if last_io_stats:
          results['sectors_read'].append(stats.num_sectors_read -
                                         last_io_stats.num_sectors_read)
          results['sectors_written'].append(stats.num_sectors_written -
                                            last_io_stats.num_sectors_written)
        last_io_stats = stats
      elif line.startswith('cpu '):
        stats = self._ParseCpuStatsLine(line)
        if last_cpu_stats:
          results['user'].append(stats.user - last_cpu_stats.user)
          results['nice'].append(stats.nice - last_cpu_stats.nice)
          results['system'].append(stats.system - last_cpu_stats.system)
          results['idle'].append(stats.idle - last_cpu_stats.idle)
          results['iowait'].append(stats.iowait - last_cpu_stats.iowait)
          results['irq'].append(stats.irq - last_cpu_stats.irq)
          results['softirq'].append(stats.softirq- last_cpu_stats.softirq)
        last_cpu_stats = stats
    units = {
      'sectors_read': 'sectors',
      'sectors_written': 'sectors',
      'user': 'jiffies',
      'nice': 'jiffies',
      'system': 'jiffies',
      'idle': 'jiffies',
      'iowait': 'jiffies',
      'irq': 'jiffies',
      'softirq': 'jiffies',
    }
    with open(output_path, 'w') as f:
      f.write('display(%d, %s, %s);' % (self._hz, json.dumps(results), units))
    return 'file://%s?results=file://%s' % (
        DeviceStatsMonitor.RESULT_VIEWER_PATH, urllib.quote(output_path))


  @staticmethod
  def _ParseCpuStatsLine(line):
    """Parses a line of cpu stats into a CpuStats named tuple."""
    # Field definitions: http://www.linuxhowtos.org/System/procstat.htm
    cpu_stats = collections.namedtuple('CpuStats',
                                       ['device',
                                        'user',
                                        'nice',
                                        'system',
                                        'idle',
                                        'iowait',
                                        'irq',
                                        'softirq',
                                       ])
    fields = line.split()
    return cpu_stats._make([fields[0]] + [int(f) for f in fields[1:8]])

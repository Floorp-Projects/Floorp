#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides iotop/top style profiling for android.

Usage:
  ./device_stats_monitor.py --hz=20 --duration=5 --outfile=/tmp/foo
"""

import optparse
import os
import sys
import time

from pylib import android_commands
from pylib import device_stats_monitor
from pylib import test_options_parser


def main(argv):
  option_parser = optparse.OptionParser()
  option_parser.add_option('--hz', type='int', default=20,
                           help='Number of samples/sec.')
  option_parser.add_option('--duration', type='int', default=5,
                           help='Seconds to monitor.')
  option_parser.add_option('--outfile', default='/tmp/devicestatsmonitor',
                           help='Location to start output file.')
  test_options_parser.AddBuildTypeOption(option_parser)
  options, args = option_parser.parse_args(argv)

  monitor = device_stats_monitor.DeviceStatsMonitor(
      android_commands.AndroidCommands(), options.hz, options.build_type)
  monitor.Start()
  print 'Waiting for %d seconds while profiling.' % options.duration
  time.sleep(options.duration)
  url = monitor.StopAndCollect(options.outfile)
  print 'View results in browser at %s' % url

if __name__ == '__main__':
  sys.exit(main(sys.argv))

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides an interface to communicate with the device via the adb command.

Assumes adb binary is currently on system path.
"""


import collections


def ParseIoStatsLine(line):
  """Parses a line of io stats into a IoStats named tuple."""
  # Field definitions: http://www.kernel.org/doc/Documentation/iostats.txt
  IoStats = collections.namedtuple('IoStats',
                                   ['device',
                                    'num_reads_issued',
                                    'num_reads_merged',
                                    'num_sectors_read',
                                    'ms_spent_reading',
                                    'num_writes_completed',
                                    'num_writes_merged',
                                    'num_sectors_written',
                                    'ms_spent_writing',
                                    'num_ios_in_progress',
                                    'ms_spent_doing_io',
                                    'ms_spent_doing_io_weighted',
                                    ])
  fields = line.split()
  return IoStats._make([fields[2]] + [int(f) for f in fields[3:]])

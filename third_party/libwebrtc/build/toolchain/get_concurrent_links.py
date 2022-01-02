#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script computs the number of concurrent links we want to run in the build
# as a function of machine spec. It's based on GetDefaultConcurrentLinks in GYP.

from __future__ import print_function

import argparse
import multiprocessing
import os
import re
import subprocess
import sys

sys.path.insert(1, os.path.join(os.path.dirname(__file__), '..'))
import gn_helpers


def _GetTotalMemoryInBytes():
  if sys.platform in ('win32', 'cygwin'):
    import ctypes

    class MEMORYSTATUSEX(ctypes.Structure):
      _fields_ = [
          ("dwLength", ctypes.c_ulong),
          ("dwMemoryLoad", ctypes.c_ulong),
          ("ullTotalPhys", ctypes.c_ulonglong),
          ("ullAvailPhys", ctypes.c_ulonglong),
          ("ullTotalPageFile", ctypes.c_ulonglong),
          ("ullAvailPageFile", ctypes.c_ulonglong),
          ("ullTotalVirtual", ctypes.c_ulonglong),
          ("ullAvailVirtual", ctypes.c_ulonglong),
          ("sullAvailExtendedVirtual", ctypes.c_ulonglong),
      ]

    stat = MEMORYSTATUSEX(dwLength=ctypes.sizeof(MEMORYSTATUSEX))
    ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(stat))
    return stat.ullTotalPhys
  elif sys.platform.startswith('linux'):
    if os.path.exists("/proc/meminfo"):
      with open("/proc/meminfo") as meminfo:
        memtotal_re = re.compile(r'^MemTotal:\s*(\d*)\s*kB')
        for line in meminfo:
          match = memtotal_re.match(line)
          if not match:
            continue
          return float(match.group(1)) * 2**10
  elif sys.platform == 'darwin':
    try:
      return int(subprocess.check_output(['sysctl', '-n', 'hw.memsize']))
    except Exception:
      return 0
  # TODO(scottmg): Implement this for other platforms.
  return 0


def _GetDefaultConcurrentLinks(per_link_gb, reserve_gb, secondary_per_link_gb):
  explanation = []
  explanation.append(
      'per_link_gb={} reserve_gb={} secondary_per_link_gb={}'.format(
          per_link_gb, reserve_gb, secondary_per_link_gb))
  # Inherit the legacy environment variable for people that have set it in GYP.
  num_links = int(os.getenv('GYP_LINK_CONCURRENCY', 0))
  if num_links:
    reason = 'GYP_LINK_CONCURRENCY'
  else:
    mem_total_gb = float(_GetTotalMemoryInBytes()) / 2**30
    mem_total_gb = max(0, mem_total_gb - reserve_gb)
    mem_cap = int(max(1, mem_total_gb / per_link_gb))
    hard_cap = max(1, int(os.getenv('GYP_LINK_CONCURRENCY_MAX', 2**32)))

    try:
      cpu_cap = multiprocessing.cpu_count()
    except:
      cpu_cap = 1

    explanation.append('cpu_count={} mem_total_gb={:.1f}GiB'.format(
        cpu_cap, mem_total_gb))

    num_links = min(mem_cap, hard_cap, cpu_cap)
    if num_links == cpu_cap:
      reason = 'cpu_count'
    elif num_links == hard_cap:
      reason = 'GYP_LINK_CONCURRENCY_MAX'
    else:
      reason = 'RAM'

  explanation.append('concurrent_links={}  (reason: {})'.format(
      num_links, reason))

  # See if there is RAM leftover for a secondary pool.
  if secondary_per_link_gb and num_links == mem_cap:
    mem_remaining = mem_total_gb - mem_cap * per_link_gb
    secondary_size = int(max(0, mem_remaining / secondary_per_link_gb))
    explanation.append('secondary_size={} (mem_remaining={:.1f}GiB)'.format(
        secondary_size, mem_remaining))
  else:
    secondary_size = 0

  return num_links, secondary_size, explanation


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--mem_per_link_gb', type=int, default=8)
  parser.add_argument('--reserve_mem_gb', type=int, default=0)
  parser.add_argument('--secondary_mem_per_link', type=int, default=0)
  options = parser.parse_args()

  primary_pool_size, secondary_pool_size, explanation = (
      _GetDefaultConcurrentLinks(options.mem_per_link_gb,
                                 options.reserve_mem_gb,
                                 options.secondary_mem_per_link))
  sys.stdout.write(
      gn_helpers.ToGNString({
          'primary_pool_size': primary_pool_size,
          'secondary_pool_size': secondary_pool_size,
          'explanation': explanation,
      }))
  return 0


if __name__ == '__main__':
  sys.exit(main())

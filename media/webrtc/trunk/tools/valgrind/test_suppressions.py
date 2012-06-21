# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import os
import re
import sys

import path_utils

import suppressions


def ReadReportsFromFile(filename):
  """ Returns a list of (report_hash, report) and the URL of the report on the
  waterfall.
  """
  input_file = file(filename, 'r')
  # reports is a list of (error hash, report) pairs.
  reports = []
  in_suppression = False
  cur_supp = []
  # This stores the last error hash found while reading the file.
  last_hash = ""
  for line in input_file:
    line = line.strip()
    line = line.replace("</span><span class=\"stdout\">", "")
    line = line.replace("</span><span class=\"stderr\">", "")
    line = line.replace("&lt;", "<")
    line = line.replace("&gt;", ">")
    if in_suppression:
      if line == "}":
        cur_supp += ["}"]
        reports += [[last_hash, "\n".join(cur_supp)]]
        in_suppression = False
        cur_supp = []
        last_hash = ""
      else:
        cur_supp += [" "*3 + line]
    elif line == "{":
      in_suppression = True
      cur_supp = ["{"]
    elif line.find("Suppression (error hash=#") == 0:
      last_hash = line[25:41]
  # The line at the end of the file is assumed to store the URL of the report.
  return reports,line


def main(argv):
  suppressions_root = path_utils.ScriptDir()
  JOIN = os.path.join

  supp_filename = JOIN(suppressions_root, "memcheck", "suppressions.txt")
  vg_common = suppressions.ReadSuppressionsFromFile(supp_filename)
  supp_filename = JOIN(suppressions_root, "tsan", "suppressions.txt")
  tsan_common = suppressions.ReadSuppressionsFromFile(supp_filename)
  common_suppressions = vg_common + tsan_common

  supp_filename = JOIN(suppressions_root, "memcheck", "suppressions_mac.txt")
  vg_mac = suppressions.ReadSuppressionsFromFile(supp_filename)
  supp_filename = JOIN(suppressions_root, "tsan", "suppressions_mac.txt")
  tsan_mac = suppressions.ReadSuppressionsFromFile(supp_filename)
  mac_suppressions = vg_mac + tsan_mac

  supp_filename = JOIN(suppressions_root, "tsan", "suppressions_win32.txt")
  tsan_win = suppressions.ReadSuppressionsFromFile(supp_filename)
  win_suppressions = tsan_win

  supp_filename = JOIN(suppressions_root, "..", "heapcheck", "suppressions.txt")
  heapcheck_suppressions = suppressions.ReadSuppressionsFromFile(supp_filename)

  supp_filename = JOIN(suppressions_root, "drmemory", "suppressions.txt")
  drmem_suppressions = suppressions.ReadSuppressionsFromFile(supp_filename)
  supp_filename = JOIN(suppressions_root, "drmemory", "suppressions_full.txt")
  drmem_full_suppressions = suppressions.ReadSuppressionsFromFile(supp_filename)

  # all_reports is a map {report: list of urls containing this report}
  all_reports = defaultdict(list)
  report_hashes = {}

  for f in argv:
    f_reports, url = ReadReportsFromFile(f)
    for (hash, report) in f_reports:
      all_reports[report] += [url]
      report_hashes[report] = hash

  reports_count = 0
  for r in all_reports:
    cur_supp = common_suppressions
    if all([re.search("%20Mac%20|mac_valgrind", url)
            for url in all_reports[r]]):
      # Include mac suppressions if the report is only present on Mac
      cur_supp += mac_suppressions
    elif all([re.search("Windows%20", url) for url in all_reports[r]]):
      # Include win32 suppressions if the report is only present on Windows
      cur_supp += win_suppressions
    elif all([re.search("%20Heapcheck", url)
              for url in all_reports[r]]):
      cur_supp += heapcheck_suppressions
    elif all(["DrMemory%20full" in url for url in all_reports[r]]):
      cur_supp += drmem_suppressions + drmem_full_suppressions
    elif all(["DrMemory" in url for url in all_reports[r]]):
      cur_supp += drmem_suppressions

    match = False
    for s in cur_supp:
      if s.Match(r.split("\n")):
        match = True
        break
    if not match:
      reports_count += 1
      print "==================================="
      print "This report observed at"
      for url in all_reports[r]:
        print "  %s" % url
      print "didn't match any suppressions:"
      print "Suppression (error hash=#%s#):" % (report_hashes[r])
      print r
      print "==================================="

  if reports_count > 0:
    print ("%d unique reports don't match any of the suppressions" %
           reports_count)
  else:
    print "Congratulations! All reports are suppressed!"
    # TODO(timurrrr): also make sure none of the old suppressions
    # were narrowed too much.


if __name__ == "__main__":
  main(sys.argv[1:])

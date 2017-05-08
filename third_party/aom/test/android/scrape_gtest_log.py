#
# Copyright (c) 2016, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and
# the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
# was not distributed with this source code in the LICENSE file, you can
# obtain it at www.aomedia.org/license/software. If the Alliance for Open
# Media Patent License 1.0 was not distributed with this source code in the
# PATENTS file, you can obtain it at www.aomedia.org/license/patent.
#

"""Standalone script which parses a gtest log for json.

Json is returned returns as an array.  This script is used by the libaom
waterfall to gather json results mixed in with gtest logs.  This is
dubious software engineering.
"""

import getopt
import json
import os
import re
import sys


def main():
  if len(sys.argv) != 3:
    print "Expects a file to write json to!"
    exit(1)

  try:
    opts, _ = \
        getopt.getopt(sys.argv[1:], \
                      'o:', ['output-json='])
  except getopt.GetOptError:
    print 'scrape_gtest_log.py -o <output_json>'
    sys.exit(2)

  output_json = ''
  for opt, arg in opts:
    if opt in ('-o', '--output-json'):
      output_json = os.path.join(arg)

  blob = sys.stdin.read()
  json_string = '[' + ','.join('{' + x + '}' for x in
                               re.findall(r'{([^}]*.?)}', blob)) + ']'
  print blob

  output = json.dumps(json.loads(json_string), indent=4, sort_keys=True)
  print output

  path = os.path.dirname(output_json)
  if path and not os.path.exists(path):
    os.makedirs(path)

  outfile = open(output_json, 'w')
  outfile.write(output)

if __name__ == '__main__':
  sys.exit(main())

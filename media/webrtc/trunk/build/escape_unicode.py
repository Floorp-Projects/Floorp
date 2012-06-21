#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert any unicode characters found in the input file to C literals."""

import codecs
import optparse
import os
import sys


def main(argv):
  parser = optparse.OptionParser()
  usage = 'Usage: %prog -o <output_dir> <input_file>'
  parser.set_usage(usage)
  parser.add_option('-o', dest='output_dir')

  options, arglist = parser.parse_args(argv)

  if not options.output_dir:
    print "output_dir required"
    return 1

  if len(arglist) != 2:
    print "input_file required"
    return 1

  in_filename = arglist[1]

  if not in_filename.endswith('.utf8'):
    print "input_file should end in .utf8"
    return 1

  out_filename = os.path.join(options.output_dir, os.path.basename(
      os.path.splitext(in_filename)[0]))

  WriteEscapedFile(in_filename, out_filename)
  return 0


def WriteEscapedFile(in_filename, out_filename):
  input_data = codecs.open(in_filename, 'r', 'utf8').read()
  with codecs.open(out_filename, 'w', 'ascii') as out_file:
    for i, char in enumerate(input_data):
      if ord(char) > 127:
        out_file.write(repr(char.encode('utf8'))[1:-1])
        if input_data[i + 1:i + 2] in '0123456789abcdefABCDEF':
          out_file.write('""')
      else:
        out_file.write(char.encode('ascii'))


if __name__ == '__main__':
  sys.exit(main(sys.argv))

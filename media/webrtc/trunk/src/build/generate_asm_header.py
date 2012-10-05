#!/usr/bin/env python
#
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""This script generates a C header file of offsets from an ARM assembler file.

It parses an ARM assembler generated .S file, finds declarations of variables
whose names start with the string specified as the third argument in the
command-line, translates the variable names and values into constant defines and
writes them into a header file.
"""

import sys

def usage():
  print("Usage: generate_asm_header.py " +
     "<input filename> <output filename> <variable name pattern>")
  sys.exit(1)

def main(argv):
  if len(argv) != 3:
    usage()

  infile = open(argv[0])
  outfile = open(argv[1], 'w')

  for line in infile:  # Iterate though all the lines in the input file.
    if line.startswith(argv[2]):
      outfile.write('#define ')
      outfile.write(line.split(':')[0])  # Write the constant name.
      outfile.write(' ')

    if line.find('.word') >= 0:
      outfile.write(line.split('.word')[1])  # Write the constant value.

  infile.close()
  outfile.close()

if __name__ == "__main__":
  main(sys.argv[1:])

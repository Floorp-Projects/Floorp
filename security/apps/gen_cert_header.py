# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import binascii

def file_byte_generator(filename, block_size = 512):
  with open(filename, "rb") as f:
    while True:
      block = f.read(block_size)
      if block:
        for byte in block:
          yield byte
      else:
        break

def create_header(array_name, in_filename):
  hexified = ["0x" + binascii.hexlify(byte) for byte in file_byte_generator(in_filename)]
  print "const uint8_t " + array_name + "[] = {"
  print ", ".join(hexified)
  print "};"
  return 0

def create_empty_header(array_name):
  # mfbt/ArrayUtils.h will not be able to pick up the
  # correct specialization for ArrayLength(const array[0])
  # so add a value of 0 which will fail cert verification
  # just the same as an empty array
  print "const uint8_t " + array_name + "[] = { 0x0 };"
  return 0

if __name__ == '__main__':
  if len(sys.argv) < 2:
    print 'ERROR: usage: gen_cert_header.py array_name in_filename'
    sys.exit(1);
  if len(sys.argv) == 2:
    sys.exit(create_empty_header(sys.argv[1]))
  sys.exit(create_header(sys.argv[1], sys.argv[2]))

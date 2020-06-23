# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function
import os

def file_byte_generator(filename, block_size = 512):
  with open(filename, "rb") as f:
    while True:
      block = f.read(block_size)
      if block:
        for byte in block:
          yield byte
      else:
        break

def create_header(out_fh, in_filename):
  assert out_fh.name.endswith('.h')
  array_name = os.path.basename(out_fh.name)[:-2] + 'Data'
  hexified = ["0x%02x" % byte for byte in file_byte_generator(in_filename)]

  print("const uint8_t " + array_name + "[] = {", file=out_fh)
  print(", ".join(hexified), file=out_fh)
  print("};", file=out_fh)
  return 0

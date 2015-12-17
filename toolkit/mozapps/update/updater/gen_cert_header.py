from __future__ import print_function

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

def create_header(out_fh, in_filename):
  assert out_fh.name.endswith('.h')
  array_name = out_fh.name[:-2] + 'Data'
  hexified = ["0x" + binascii.hexlify(byte) for byte in file_byte_generator(in_filename)]
  print("const uint8_t " + array_name + "[] = {", file=out_fh)
  print(", ".join(hexified), file=out_fh)
  print("};", file=out_fh)
  return 0

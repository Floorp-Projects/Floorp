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

if __name__ == '__main__':
  if len(sys.argv) < 3:
    print 'ERROR: usage: gen_cert_header.py array_name in_filename'
    sys.exit(1);
  sys.exit(create_header(sys.argv[1], sys.argv[2]))

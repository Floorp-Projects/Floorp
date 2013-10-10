# This script exists to auto-generate Http2HuffmanOutgoing.h from the table
# contained in the HPACK spec. It's pretty simple to run:
#   python make_outgoing_tables.py < huff_outgoing.txt > Http2HuffmanOutgoing.h
# where huff_outgoing.txt is copy/pasted text from the latest version of the
# HPACK spec, with all non-relevant lines removed (the most recent version
# of huff_outgoing.txt also lives in this directory as an example).
import sys

sys.stdout.write('''/*
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT!
 */
#ifndef mozilla__net__Http2HuffmanOutgoing_h
#define mozilla__net__Http2HuffmanOutging_h

namespace mozilla {
namespace net {

struct HuffmanOutgoingEntry {
  uint8_t mLength;
  uint32_t mValue;
};

static HuffmanOutgoingEntry HuffmanOutgoing[] = {
''')

entries = []
for line in sys.stdin:
    line = line.strip()
    obracket = line.rfind('[')
    nbits = int(line[obracket + 1:-1])

    encend = obracket - 1
    hexits = nbits / 4
    if hexits * 4 != nbits:
        hexits += 1

    enc = line[encend - hexits:encend]
    val = int(enc, 16)

    entries.append({'length': nbits, 'value': val})

line = []
for i, e in enumerate(entries):
    sys.stdout.write('  { %s, 0x%08x }' %
                     (e['length'], e['value']))
    if i < (len(entries) - 1):
        sys.stdout.write(',')
    sys.stdout.write('\n')

sys.stdout.write('''};

} // namespace net
} // namespace mozilla

#endif // mozilla__net__Http2HuffmanOutgoing_h
''')


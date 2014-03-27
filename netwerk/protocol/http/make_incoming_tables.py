# This script exists to auto-generate Http2HuffmanIncoming.h from the table
# contained in the HPACK spec. It's pretty simple to run:
#   python make_incoming_tables.py < http2_huffman_table.txt > Http2HuffmanIncoming.h
# where huff_incoming.txt is copy/pasted text from the latest version of the
# HPACK spec, with all non-relevant lines removed (the most recent version
# of huff_incoming.txt also lives in this directory as an example).
import sys

def char_cmp(x, y):
    rv = cmp(x['nbits'], y['nbits'])
    if not rv:
        rv = cmp(x['bpat'], y['bpat'])
    if not rv:
        rv = cmp(x['ascii'], y['ascii'])
    return rv

characters = []

for line in sys.stdin:
    line = line.rstrip()
    obracket = line.rfind('[')
    nbits = int(line[obracket + 1:-1])

    ascii = int(line[10:13].strip())

    bar = line.find('|', 9)
    obracket = line.find('[', bar)
    bpat = line[bar + 1:obracket - 1].strip().rstrip('|')

    characters.append({'ascii': ascii, 'nbits': nbits, 'bpat': bpat})

characters.sort(cmp=char_cmp)
raw_entries = []
for c in characters:
    raw_entries.append((c['ascii'], c['bpat']))

class DefaultList(list):
    def __init__(self, default=None):
        self.__default = default

    def __ensure_size(self, sz):
        while sz > len(self):
            self.append(self.__default)

    def __getitem__(self, idx):
        self.__ensure_size(idx + 1)
        rv = super(DefaultList, self).__getitem__(idx)
        return rv

    def __setitem__(self, idx, val):
        self.__ensure_size(idx + 1)
        super(DefaultList, self).__setitem__(idx, val)

def expand_to_8bit(bstr):
    while len(bstr) < 8:
        bstr += '0'
    return int(bstr, 2)

table = DefaultList()
for r in raw_entries:
    ascii, bpat = r
    ascii = int(ascii)
    bstrs = bpat.split('|')
    curr_table = table
    while len(bstrs) > 1:
        idx = expand_to_8bit(bstrs[0])
        if curr_table[idx] is None:
            curr_table[idx] = DefaultList()
        curr_table = curr_table[idx]
        bstrs.pop(0)

    idx = expand_to_8bit(bstrs[0])
    curr_table[idx] = {'prefix_len': len(bstrs[0]),
                        'mask': int(bstrs[0], 2),
                        'value': ascii}


def output_table(table, name_suffix=''):
    max_prefix_len = 0
    for i, t in enumerate(table):
        if isinstance(t, dict):
            if t['prefix_len'] > max_prefix_len:
                max_prefix_len = t['prefix_len']
        elif t is not None:
            output_table(t, '%s_%s' % (name_suffix, i))

    tablename = 'HuffmanIncoming%s' % (name_suffix if name_suffix else 'Root',)
    entriestable = tablename.replace('HuffmanIncoming', 'HuffmanIncomingEntries')
    sys.stdout.write('static HuffmanIncomingEntry %s[] = {\n' % (entriestable,))
    prefix_len = 0
    value = 0
    ptr = 'nullptr'
    for i in range(256):
        t = table[i]
        if isinstance(t, dict):
            prefix_len = t['prefix_len']
            value = t['value']
            ptr = 'nullptr'
        elif t is not None:
            prefix_len = 0
            value = 0
            subtable = '%s_%s' % (name_suffix, i)
            ptr = '&HuffmanIncoming%s' % (subtable,)
        sys.stdout.write('  { %s, %s, %s }' %
                         (ptr, value, prefix_len))
        if i < 255:
            sys.stdout.write(',')
        sys.stdout.write('\n')
    sys.stdout.write('};\n')
    sys.stdout.write('\n')
    sys.stdout.write('static HuffmanIncomingTable %s = {\n' % (tablename,))
    sys.stdout.write('  %s,\n' % (entriestable,))
    sys.stdout.write('  %s\n' % (max_prefix_len,))
    sys.stdout.write('};\n')
    sys.stdout.write('\n')

sys.stdout.write('''/*
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT!
 */
#ifndef mozilla__net__Http2HuffmanIncoming_h
#define mozilla__net__Http2HuffmanIncoming_h

namespace mozilla {
namespace net {

struct HuffmanIncomingTable;

struct HuffmanIncomingEntry {
  HuffmanIncomingTable *mPtr;
  uint16_t mValue;
  uint8_t mPrefixLen;
};

struct HuffmanIncomingTable {
  HuffmanIncomingEntry *mEntries;
  uint8_t mPrefixLen;
};

''')

output_table(table)

sys.stdout.write('''} // namespace net
} // namespace mozilla

#endif // mozilla__net__Http2HuffmanIncoming_h
''')

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

    oparen = line.find(' (')
    ascii = int(line[oparen + 2:oparen + 5].strip())

    bar = line.find('|', oparen)
    space = line.find(' ', bar)
    bpat = line[bar + 1:space].strip().rstrip('|')

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
    nexttable = tablename.replace('HuffmanIncoming', 'HuffmanIncomingNextTables')
    sys.stdout.write('static const HuffmanIncomingEntry %s[] = {\n' %
                     (entriestable,))
    prefix_len = 0
    value = 0
    i = 0
    while i < 256:
        t = table[i]
        if isinstance(t, dict):
            value = t['value']
            prefix_len = t['prefix_len']
        elif t is not None:
            break

        sys.stdout.write('  { %s, %s }' %
                         (value, prefix_len))
        sys.stdout.write(',\n')
        i += 1

    indexOfFirstNextTable = i
    if i < 256:
        sys.stdout.write('};\n')
        sys.stdout.write('\n')
        sys.stdout.write('static const HuffmanIncomingTable* %s[] = {\n' %
                         (nexttable,))
        while i < 256:
            subtable = '%s_%s' % (name_suffix, i)
            ptr = '&HuffmanIncoming%s' % (subtable,)
            sys.stdout.write('  %s' %
                             (ptr))
            sys.stdout.write(',\n')
            i += 1
    else:
        nexttable = 'nullptr'

    sys.stdout.write('};\n')
    sys.stdout.write('\n')
    sys.stdout.write('static const HuffmanIncomingTable %s = {\n' % (tablename,))
    sys.stdout.write('  %s,\n' % (entriestable,))
    sys.stdout.write('  %s,\n' % (nexttable,))
    sys.stdout.write('  %s,\n' % (indexOfFirstNextTable,))
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
  const uint16_t mValue:9;      // 9 bits so it can hold 0..256
  const uint16_t mPrefixLen:7;  // only holds 1..8
};

// The data members are public only so they can be statically constructed. All
// accesses should be done through the functions.
struct HuffmanIncomingTable {
  // The normal entries, for indices in the range 0..(mNumEntries-1).
  const HuffmanIncomingEntry* const mEntries;

  // The next tables, for indices in the range mNumEntries..255. Must be
  // |nullptr| if mIndexOfFirstNextTable is 256.
  const HuffmanIncomingTable** const mNextTables;

  // The index of the first next table (equal to the number of entries in
  // mEntries). This cannot be a uint8_t because it can have the value 256,
  // in which case there are no next tables and mNextTables must be |nullptr|.
  const uint16_t mIndexOfFirstNextTable;

  const uint8_t mPrefixLen;

  bool IndexHasANextTable(uint8_t aIndex) const
  {
    return aIndex >= mIndexOfFirstNextTable;
  }

  const HuffmanIncomingEntry* Entry(uint8_t aIndex) const
  {
    MOZ_ASSERT(aIndex < mIndexOfFirstNextTable);
    return &mEntries[aIndex];
  }

  const HuffmanIncomingTable* NextTable(uint8_t aIndex) const
  {
    MOZ_ASSERT(aIndex >= mIndexOfFirstNextTable);
    return mNextTables[aIndex - mIndexOfFirstNextTable];
  }
};

''')

output_table(table)

sys.stdout.write('''} // namespace net
} // namespace mozilla

#endif // mozilla__net__Http2HuffmanIncoming_h
''')

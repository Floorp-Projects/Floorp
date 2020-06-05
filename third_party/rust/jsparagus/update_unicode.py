#!/usr/bin/env python3

""" Generate Unicode data table for parser
"""

import argparse
import io
import re
import sys
from contextlib import closing
from itertools import tee, zip_longest
from urllib.request import urlopen
from zipfile import ZipFile


# These are also part of IdentifierPart §11.6 Names and Keywords
compatibility_identifier_part = [
    ord(u'\N{ZERO WIDTH NON-JOINER}'),
    ord(u'\N{ZERO WIDTH JOINER}'),
]

FLAG_ID_START = 1 << 0
FLAG_ID_CONTINUE = 1 << 1


def download_derived_core_properties(version):
    """Downloads UCD.zip for given version, and return the content of
    DerivedCoreProperties.txt. """

    baseurl = 'https://unicode.org/Public'
    if version == 'UNIDATA':
        url = '%s/%s' % (baseurl, version)
    else:
        url = '%s/%s/ucd' % (baseurl, version)

    request_url = '{}/UCD.zip'.format(url)
    with closing(urlopen(request_url)) as downloaded_file:
        downloaded_data = io.BytesIO(downloaded_file.read())

    with ZipFile(downloaded_data) as zip_file:
        return zip_file.read('DerivedCoreProperties.txt').decode()


def read_derived_core_properties(derived_core_properties):
    """Read DerivedCoreProperties.txt content and yield each item. """
    for line in derived_core_properties.split('\n'):
        if line == '' or line.startswith('#'):
            continue
        row = line.split('#')[0].split(';')
        char_range = row[0].strip()
        char_property = row[1].strip()
        if '..' not in char_range:
            yield (int(char_range, 16), char_property)
        else:
            [start, end] = char_range.split('..')
            for char in range(int(start, 16), int(end, 16) + 1):
                yield (char, char_property)


def process_derived_core_properties(derived_core_properties):
    """Parse DerivedCoreProperties.txt and returns its version,
    and set of characters with ID_Start and ID_Continue. """
    id_start = set()
    id_continue = set()

    m = re.match('# DerivedCoreProperties-([0-9\.]+).txt', derived_core_properties)
    assert m
    version = m.group(1)

    for (char, prop) in read_derived_core_properties(derived_core_properties):
        if prop == 'ID_Start':
            id_start.add(char)
        if prop == 'ID_Continue':
            id_continue.add(char)

    return (version, id_start, id_continue)


def int_ranges(ints):
    """ Yields consecutive ranges (inclusive) from integer values. """
    (a, b) = tee(sorted(ints))
    start = next(b)
    for (curr, succ) in zip_longest(a, b):
        if curr + 1 != succ:
            yield (start, curr)
            start = succ


def process_unicode_data(derived_core_properties):
    MAX_BMP = 0xffff

    dummy = 0
    table = [dummy]
    cache = {dummy: 0}
    index = [0] * (MAX_BMP + 1)
    non_bmp_id_start_set = {}
    non_bmp_id_continue_set = {}

    (version, id_start, id_continue) = process_derived_core_properties(derived_core_properties)
    codes = id_start.union(id_continue)

    for code in codes:
        if code > MAX_BMP:
            if code in id_start:
                non_bmp_id_start_set[code] = 1
            if code in id_continue:
                non_bmp_id_continue_set[code] = 1
            continue

        flags = 0
        if code in id_start:
            flags |= FLAG_ID_START
        if code in id_continue or code in compatibility_identifier_part:
            flags |= FLAG_ID_CONTINUE

        i = cache.get(flags)
        if i is None:
            assert flags not in table
            cache[flags] = i = len(table)
            table.append(flags)
        index[code] = i

    return (
        version,
        table,
        index,
        id_start,
        id_continue,
        non_bmp_id_start_set,
        non_bmp_id_continue_set,
    )


def getsize(data):
    """ return smallest possible integer size for the given array """
    maxdata = max(data)
    assert maxdata < 2**32

    if maxdata < 256:
        return 1
    elif maxdata < 65536:
        return 2
    else:
        return 4


def splitbins(t):
    """t -> (t1, t2, shift).  Split a table to save space.

    t is a sequence of ints.  This function can be useful to save space if
    many of the ints are the same.  t1 and t2 are lists of ints, and shift
    is an int, chosen to minimize the combined size of t1 and t2 (in C
    code), and where for each i in range(len(t)),
        t[i] == t2[(t1[i >> shift] << shift) + (i & mask)]
    where mask is a bitmask isolating the last "shift" bits.
    """

    def dump(t1, t2, shift, bytes):
        print("%d+%d bins at shift %d; %d bytes" % (
            len(t1), len(t2), shift, bytes), file=sys.stderr)
        print("Size of original table:", len(t) * getsize(t),
              "bytes", file=sys.stderr)


    n = len(t)-1    # last valid index
    maxshift = 0    # the most we can shift n and still have something left
    if n > 0:
        while n >> 1:
            n >>= 1
            maxshift += 1
    del n
    bytes = sys.maxsize  # smallest total size so far
    t = tuple(t)    # so slices can be dict keys
    for shift in range(maxshift + 1):
        t1 = []
        t2 = []
        size = 2**shift
        bincache = {}

        for i in range(0, len(t), size):
            bin = t[i:i + size]

            index = bincache.get(bin)
            if index is None:
                index = len(t2)
                bincache[bin] = index
                t2.extend(bin)
            t1.append(index >> shift)

        # determine memory size
        b = len(t1) * getsize(t1) + len(t2) * getsize(t2)
        if b < bytes:
            best = t1, t2, shift
            bytes = b
    t1, t2, shift = best

    print("Best:", end=' ', file=sys.stderr)
    dump(t1, t2, shift, bytes)

    # exhaustively verify that the decomposition is correct
    mask = 2**shift - 1
    for i in range(len(t)):
        assert t[i] == t2[(t1[i >> shift] << shift) + (i & mask)]
    return best


def write_table(f, name, type, table, formatter, per_line):
    f.write(f"""
pub const {name}: &'static [{type}] = &[
""")

    i = 0
    for item in table:
        if i == 0:
            f.write('    ')
        f.write(f'{formatter(item)},')
        i += 1
        if i == per_line:
            i = 0
            f.write("""
""")

    f.write("""\
];
""")


def write_func(f, name, group_set):
        f.write(f"""
pub fn {name}(c: char) -> bool {{""")

        for (from_code, to_code) in int_ranges(group_set.keys()):
            f.write(f"""
    if c >= \'\\u{{{from_code:X}}}\' && c <= \'\\u{{{to_code:X}}}\' {{
        return true;
    }}""")

        f.write("""
    false
}
""")


def make_unicode_file(version, table, index,
                      id_start, id_continue,
                      non_bmp_id_start_set, non_bmp_id_continue_set):
    index1, index2, shift = splitbins(index)

    # verify correctness
    for char in index:
        test = table[index[char]]

        idx = index1[char >> shift]
        idx = index2[(idx << shift) + (char & ((1 << shift) - 1))]

        assert test == table[idx]

    with open('crates/parser/src/unicode_data.rs', 'w') as f:
        f.write(f"""\
// Generated by update_unicode.py DO NOT MODIFY
// Unicode version: {version}
""")

        f.write(f"""
const FLAG_ID_START: u8 = {FLAG_ID_START};
const FLAG_ID_CONTINUE: u8 = {FLAG_ID_CONTINUE};
""")

        f.write("""
pub struct CharInfo {
    flags: u8,
}

impl CharInfo {
    pub fn is_id_start(&self) -> bool {
        self.flags & FLAG_ID_START != 0
    }

    pub fn is_id_continue(&self) -> bool {
        self.flags & FLAG_ID_CONTINUE != 0
    }
}
""")

        write_table(f, 'CHAR_INFO_TABLE', 'CharInfo', table,
                    lambda flag: f"CharInfo {{ flags: {flag} }}",
                    1)
        write_table(f, 'INDEX1', 'u8', index1,
                    lambda i: f'{i:4d}', 8)
        write_table(f, 'INDEX2', 'u8', index2,
                    lambda i: f'{i:4d}', 8)

        f.write(f"""
const SHIFT: usize = {shift};
""")

        f.write("""
pub fn char_info(c: char) -> &'static CharInfo {
    let code = c as usize;
    let index = INDEX1[code >> SHIFT] as usize;
    let index = INDEX2[(index << SHIFT) + (code & ((1 << SHIFT) - 1))] as usize;

    &CHAR_INFO_TABLE[index]
}
""")

        def format_bool(b):
            if b:
                return 'true '
            else:
                return 'false'

        write_table(f, 'IS_ID_START_TABLE', 'bool', range(0, 128),
                    lambda code: format_bool(code in id_start), 8)
        write_table(f, 'IS_ID_CONTINUE_TABLE', 'bool', range(0, 128),
                    lambda code: format_bool(code in id_continue), 8)

        write_func(f, 'is_id_start_non_bmp', non_bmp_id_start_set)
        write_func(f, 'is_id_continue_non_bmp', non_bmp_id_continue_set)



parser = argparse.ArgumentParser(description='Generate Unicode data table for parser')
parser.add_argument('VERSION',
                    help='Unicode version number to download from\
                    <https://unicode.org/Public>. The number must match\
                    a published Unicode version, e.g. use\
                    "--version=8.0.0" to download Unicode 8 files. Alternatively use\
                    "--version=UNIDATA" to download the latest published version.')
parser.add_argument('PATH_TO_JSPARAGUS',
                    help='Path to jsparagus')
args = parser.parse_args()

derived_core_properties = download_derived_core_properties(args.VERSION)

(
    version,
    table,
    index,
    id_start,
    id_continue,
    non_bmp_id_start_set,
    non_bmp_id_continue_set,
) = process_unicode_data(derived_core_properties)

make_unicode_file(
    version,
    table,
    index,
    id_start,
    id_continue,
    non_bmp_id_start_set,
    non_bmp_id_continue_set,
)

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script returns the number of items for the DW_AT_ranges corresponding
# to a given compilation unit. This is used as a helper to find a bug in some
# versions of GNU ld.

import subprocess
import sys
import re

def get_range_for(compilation_unit, debug_info):
    '''Returns the range offset for a given compilation unit
       in a given debug_info.'''
    name = ranges = ''
    search_cu = False
    for nfo in debug_info.splitlines():
        if 'DW_TAG_compile_unit' in nfo:
            search_cu = True
        elif 'DW_TAG_' in nfo or not nfo.strip():
            if name == compilation_unit and ranges != '':
                return int(ranges, 16)
            name = ranges = ''
            search_cu = False
        if search_cu:
            if 'DW_AT_name' in nfo:
                name = nfo.rsplit(None, 1)[1]
            elif 'DW_AT_ranges' in nfo:
                ranges = nfo.rsplit(None, 1)[1]
    return None

def get_range_length(range, debug_ranges):
    '''Returns the number of items in the range starting at the
       given offset.'''
    length = 0
    for line in debug_ranges.splitlines():
        m = re.match('\s*([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)', line)
        if m and int(m.group(1), 16) == range:
            length += 1
    return length

def main(bin, compilation_unit):
    p = subprocess.Popen(['objdump', '-W', bin], stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    (out, err) = p.communicate()
    sections = re.split('\n(Contents of the|The section) ', out)
    debug_info = [s for s in sections if s.startswith('.debug_info')]
    debug_ranges = [s for s in sections if s.startswith('.debug_ranges')]
    if not debug_ranges or not debug_info:
        return 0

    range = get_range_for(compilation_unit, debug_info[0])
    if range is not None:
        return get_range_length(range, debug_ranges[0])

    return -1


if __name__ == '__main__':
    print main(*sys.argv[1:])

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import hashlib
from mozpack.packager.unpack import UnpackFinder
from mozpack.files import DeflatedFile
from collections import OrderedDict

'''
Find files duplicated in a given packaged directory, independently of its
package format.
'''


def find_dupes(source):
    md5s = OrderedDict()
    for p, f in UnpackFinder(source):
        content = f.open().read()
        m = hashlib.md5(content).digest()
        if m not in md5s:
            if isinstance(f, DeflatedFile):
                compressed = f.file.compressed_size
            else:
                compressed = len(content)
            md5s[m] = (len(content), compressed, [])
        md5s[m][2].append(p)
    total = 0
    total_compressed = 0
    num_dupes = 0
    for m, (size, compressed, paths) in sorted(md5s.iteritems(),
                                               key=lambda x: x[1][1]):
        if len(paths) > 1:
            print 'Duplicates %d bytes%s%s:' % (size,
                  ' (%d compressed)' % compressed if compressed != size else '',
                  ' (%d times)' % (len(paths) - 1) if len(paths) > 2 else '')
            print ''.join('  %s\n' % p for p in paths)
            total += (len(paths) - 1) * size
            total_compressed += (len(paths) - 1) * compressed
            num_dupes += 1
    if num_dupes:
        print "WARNING: Found %d duplicated files taking %d bytes (%s)" % \
              (num_dupes, total,
               '%d compressed' % total_compressed if total_compressed != total
                                                  else 'uncompressed')


def main():
    if len(sys.argv) != 2:
        import os
        print >>sys.stderr, "Usage: %s directory" % \
                            os.path.basename(sys.argv[0])
        sys.exit(1)

    find_dupes(sys.argv[1])

if __name__ == "__main__":
    main()

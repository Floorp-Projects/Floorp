# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import sys
import os

from mozpack.files import FileFinder
from mozpack.mozjar import JarWriter

def make_archive(archive_name, base, exclude, include):
    finder = FileFinder(base, ignore=exclude)
    if not include:
        include = ['*']

    archive_basename = os.path.basename(archive_name)
    with open(archive_name, 'wb') as fh:
        with JarWriter(fileobj=fh, optimize=False, compress_level=5) as writer:
            for pat in include:
                for p, f in finder.find(pat):
                    print('  Adding to "%s":\n\t"%s"' % (archive_basename, p))
                    writer.add(p.encode('utf-8'), f.read(), mode=f.mode, skip_duplicates=True)

def main(argv):
    parser = argparse.ArgumentParser(description='Produce a symbols archive')
    parser.add_argument('archive', help='Which archive to generate')
    parser.add_argument('base', help='Base directory to package')
    parser.add_argument('--exclude', default=[], action='append', help='File patterns to exclude')
    parser.add_argument('--include', default=[], action='append', help='File patterns to include')

    args = parser.parse_args(argv)

    make_archive(args.archive, args.base, args.exclude, args.include)

if __name__ == '__main__':
    main(sys.argv[1:])

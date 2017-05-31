# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import sys
import os

from mozpack.files import FileFinder
from mozpack.mozjar import JarWriter
import mozpack.path as mozpath

def make_archive(archive_name, base, exclude, include, compress):
    finder = FileFinder(base, ignore=exclude)
    if not include:
        include = ['*']
    if not compress:
        compress = ['**/*.sym']
    archive_basename = os.path.basename(archive_name)
    with open(archive_name, 'wb') as fh:
        with JarWriter(fileobj=fh, optimize=False, compress_level=5) as writer:
            for pat in include:
                for p, f in finder.find(pat):
                    print('  Adding to "%s":\n\t"%s"' % (archive_basename, p))
                    should_compress = any(mozpath.match(p, pat) for pat in compress)
                    writer.add(p.encode('utf-8'), f, mode=f.mode,
                               compress=should_compress, skip_duplicates=True)

def main(argv):
    parser = argparse.ArgumentParser(description='Produce a symbols archive')
    parser.add_argument('archive', help='Which archive to generate')
    parser.add_argument('base', help='Base directory to package')
    parser.add_argument('--exclude', default=[], action='append', help='File patterns to exclude')
    parser.add_argument('--include', default=[], action='append', help='File patterns to include')
    parser.add_argument('--compress', default=[], action='append', help='File patterns to compress')

    args = parser.parse_args(argv)

    make_archive(args.archive, args.base, args.exclude, args.include, args.compress)

if __name__ == '__main__':
    main(sys.argv[1:])

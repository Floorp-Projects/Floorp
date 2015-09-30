# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This action is used to produce test archives.
#
# Ideally, the data in this file should be defined in moz.build files.
# It is defined inline because this was easiest to make test archive
# generation faster.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import os
import sys

from mozpack.files import FileFinder
from mozpack.mozjar import JarWriter
import mozpack.path as mozpath

import buildconfig

STAGE = mozpath.join(buildconfig.topobjdir, 'dist', 'test-stage')


ARCHIVE_FILES = {
    'mochitest': [
        {
            'source': buildconfig.topobjdir,
            'base': '_tests/testing',
            'pattern': 'mochitest/**',
        },
        {
            'source': STAGE,
            'base': '',
            'pattern': 'mochitest/**',
        },
    ],
    'mozharness': [
        {
            'source': buildconfig.topsrcdir,
            'base': 'testing',
            'pattern': 'mozharness/**',
        },
    ],
    'xpcshell': [
        {
            'source': buildconfig.topobjdir,
            'base': '_tests/xpcshell',
            'pattern': '**',
            'dest': 'xpcshell/tests',
        },
        {
            'source': STAGE,
            'base': '',
            'pattern': 'xpcshell/**',
        },
    ],
}


def find_files(archive):
    for entry in ARCHIVE_FILES[archive]:
        source = entry['source']
        base = entry['base']
        pattern = entry['pattern']
        dest = entry.get('dest')
        ignore = list(entry.get('ignore', []))
        ignore.append('**/.mkdir.done')
        ignore.append('**/*.pyc')

        common_kwargs = {
            'find_executables': False,
            'find_dotfiles': True,
            'ignore': ignore,
        }

        finder = FileFinder(os.path.join(source, base), **common_kwargs)

        for p, f in finder.find(pattern):
            if dest:
                p = mozpath.join(dest, p)
            yield p, f


def main(argv):
    parser = argparse.ArgumentParser(
        description='Produce test archives')
    parser.add_argument('archive', help='Which archive to generate')
    parser.add_argument('outputfile', help='File to write output to')

    args = parser.parse_args(argv)

    if not args.outputfile.endswith('.zip'):
        raise Exception('expected zip output file')

    with open(args.outputfile, 'wb') as fh:
        with JarWriter(fileobj=fh, optimize=False) as writer:
            res = find_files(args.archive)
            for p, f in res:
                writer.add(p.encode('utf-8'), f.read(), mode=f.mode)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

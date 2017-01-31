# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import argparse
import sys

from mozpack.files import FileFinder
from mozpack.copier import Jarrer

def package_gcno_tree(root, output_file):
    # XXX JarWriter doesn't support unicode strings, see bug 1056859
    if isinstance(root, unicode):
        root = root.encode('utf-8')

    finder = FileFinder(root)
    jarrer = Jarrer(optimize=False)
    for p, f in finder.find("**/*.gcno"):
        jarrer.add(p, f)
    jarrer.copy(output_file)


def cli(args=sys.argv[1:]):
    parser = argparse.ArgumentParser()
    parser.add_argument('-o', '--output-file',
                        dest='output_file',
                        help='Path to save packaged data to.')
    parser.add_argument('--root',
                        dest='root',
                        default=None,
                        help='Root directory to search from.')
    args = parser.parse_args(args)

    if not args.root:
        from buildconfig import topobjdir
        args.root = topobjdir

    return package_gcno_tree(args.root, args.output_file)

if __name__ == '__main__':
    sys.exit(cli())

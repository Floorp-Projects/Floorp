# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import json
import os.path
import sys

import buildconfig


def main(argv):
    parser = argparse.ArgumentParser(
        description='Produces a JSON manifest of built-in add-ons')
    parser.add_argument('--features', type=str, dest='featuresdir',
                        action='store', help=('The distribution sub-directory '
                                              'containing feature add-ons'))
    parser.add_argument('outputfile', help='File to write output to')
    args = parser.parse_args(argv)

    bindir = os.path.join(buildconfig.topobjdir, 'dist/bin')

    def find_dictionaries(path):
        dicts = {}
        for filename in os.listdir(os.path.join(bindir, path)):
            base, ext = os.path.splitext(filename)
            if ext == '.dic':
                dicts[base] = '%s/%s' % (path, filename)
        return dicts

    listing = {
        "dictionaries": find_dictionaries("dictionaries"),
    }
    if args.featuresdir:
        listing["system"] = sorted(os.listdir(os.path.join(bindir,
                                                           args.featuresdir)))

    with open(os.path.join(bindir, args.outputfile), 'w') as fh:
        json.dump(listing, fh, sort_keys=True)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

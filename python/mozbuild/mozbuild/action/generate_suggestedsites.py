# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

''' Script to generate the suggestedsites.json file for Fennec.

This script follows these steps:

1. Looks for a 'list.txt' file in one of the given source directories
(see srcdir). The list.txt contains the list of site names, one per line.

2. For each site name found in 'list.txt', it tries to find a matching
.json file in one of the source directories.

3. For each json file, load it and define the respective imageurl
based on a image URL template composed by the target Android package name
and the site name.

4. Join the JSON representation of each site into a JSON array and write
the result to suggestedsites.json on the locale-specific raw resource
directory e.g. raw/suggestedsites.json, raw-pt-rBR/suggestedsites.json.
'''

from __future__ import print_function

import argparse
import json
import sys
import os

from mozbuild.util import (
    FileAvoidWrite,
)
from mozpack.files import (
    FileFinder,
)
import mozpack.path as mozpath


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--verbose', '-v', default=False, action='store_true',
                        help='be verbose')
    parser.add_argument('--silent', '-s', default=False, action='store_true',
                        help='be silent')
    parser.add_argument('--android-package-name', metavar='NAME',
                        required=True,
                        help='Android package name')
    parser.add_argument('--resources', metavar='RESOURCES',
                        default=None,
                        help='optional Android resource directory to find drawables in')
    parser.add_argument('--srcdir', metavar='SRCDIR',
                        action='append', required=True,
                        help='directories to read inputs from, in order of priority')
    parser.add_argument('output', metavar='OUTPUT',
                        help='output')
    opts = parser.parse_args(args)

    def resolve_filename(filename):
        for srcdir in opts.srcdir:
            path = mozpath.join(srcdir, filename)
            if os.path.exists(path):
                return path
        return None

    # The list.txt file has one site name per line.
    names = [s.strip() for s in open(resolve_filename('list.txt'), 'rt').readlines()]
    if opts.verbose:
        print('Reading {len} suggested sites: {names}'.format(len=len(names), names=names))

    # Keep these two in sync.
    image_url_template = 'android.resource://%s/drawable/suggestedsites_{name}' % opts.android_package_name
    drawables_template = 'drawable*/suggestedsites_{name}.*'

    # Load json files corresponding to each site name and define their
    # respective image URL.
    sites = []
    for name in names:
        filename = resolve_filename(name + '.json')
        if opts.verbose:
            print("Reading '{name}' from {filename}"
                .format(name=name, filename=filename))
        site = json.load(open(filename, 'rt'))
        site['imageurl'] = image_url_template.format(name=name)
        sites.append(site)

        # Now check for existence of an appropriately named drawable.  If none
        # exists, throw.  This stops a locale discovering, at runtime, that the
        # corresponding drawable was not added to en-US.
        if not opts.resources:
            continue
        resources = os.path.abspath(opts.resources)
        finder = FileFinder(resources)
        matches = [p for p, _ in finder.find(drawables_template.format(name=name))]
        if not matches:
            raise Exception("Could not find drawable in '{resources}' for '{name}'"
                .format(resources=resources, name=name))
        else:
            if opts.verbose:
                print("Found {len} drawables in '{resources}' for '{name}': {matches}"
                      .format(len=len(matches), resources=resources, name=name, matches=matches))

    # FileAvoidWrite creates its parent directories.
    output = os.path.abspath(opts.output)
    fh = FileAvoidWrite(output)
    json.dump(sites, fh)
    existed, updated = fh.close()

    if not opts.silent:
        if updated:
            print('{output} updated'.format(output=output))
        else:
            print('{output} already up-to-date'.format(output=output))

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

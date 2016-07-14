# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

''' Script to generate the suggestedsites.json file for Fennec.

This script follows these steps:

1. Read the region.properties file in all the given source directories
(see srcdir option). Merge all properties into a single dict accounting for
the priority of source directories.

2. Read the list of sites from the list 'browser.suggestedsites.list.INDEX' and
'browser.suggestedsites.restricted.list.INDEX' properties with value of these keys
being an identifier for each suggested site e.g. browser.suggestedsites.list.0=mozilla,
browser.suggestedsites.list.1=fxmarketplace.

3. For each site identifier defined by the list keys, look for matching branches
containing the respective properties i.e. url, title, etc. For example,
for a 'mozilla' identifier, we'll look for keys like:
browser.suggestedsites.mozilla.url, browser.suggestedsites.mozilla.title, etc.

4. Generate a JSON representation of each site, join them in a JSON array, and
write the result to suggestedsites.json on the locale-specific raw resource
directory e.g. raw/suggestedsites.json, raw-pt-rBR/suggestedsites.json.
'''

from __future__ import absolute_import, print_function

import argparse
import copy
import json
import sys
import os

from mozbuild.dotproperties import (
    DotProperties,
)
from mozbuild.util import (
    FileAvoidWrite,
)
from mozpack.files import (
    FileFinder,
)
import mozpack.path as mozpath


def merge_properties(filename, srcdirs):
    """Merges properties from the given file in the given source directories."""
    properties = DotProperties()
    for srcdir in srcdirs:
        path = mozpath.join(srcdir, filename)
        try:
            properties.update(path)
        except IOError:
            # Ignore non-existing files
            continue
    return properties


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

    # Use reversed order so that the first srcdir has higher priority to override keys.
    properties = merge_properties('region.properties', reversed(opts.srcdir))

    # Keep these two in sync.
    image_url_template = 'android.resource://%s/drawable/suggestedsites_{name}' % opts.android_package_name
    drawables_template = 'drawable*/suggestedsites_{name}.*'

    # Load properties corresponding to each site name and define their
    # respective image URL.
    sites = []

    def add_names(names, defaults={}):
        for name in names:
            site = copy.deepcopy(defaults)
            site.update(properties.get_dict('browser.suggestedsites.{name}'.format(name=name), required_keys=('title', 'url', 'bgcolor')))
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

    # We want the lists to be ordered for reproducibility.  Each list has a
    # "default" JSON list item which will be extended by the properties read.
    lists = [
        ('browser.suggestedsites.list', {}),
        ('browser.suggestedsites.restricted.list', {'restricted': True}),
    ]
    if opts.verbose:
        print('Reading {len} suggested site lists: {lists}'.format(len=len(lists), lists=[list_name for list_name, _ in lists]))

    for (list_name, list_item_defaults) in lists:
        names = properties.get_list(list_name)
        if opts.verbose:
            print('Reading {len} suggested sites from {list}: {names}'.format(len=len(names), list=list_name, names=names))
        add_names(names, list_item_defaults)


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

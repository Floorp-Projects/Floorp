# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Script to generate the browsersearch.json file for Fennec.

This script follows these steps:

1. Read all the given region.properties files (see inputs and --fallback
options). Merge all properties into a single dict accounting for the priority
of inputs.

2. Read the default search plugin from 'browser.search.defaultenginename'.

3. Read the list of search plugins from the 'browser.search.order.INDEX'
properties with values identifying particular search plugins by name.

4. Read each region-specific default search plugin from each property named like
'browser.search.defaultenginename.REGION'.

5. Read the list of region-specific search plugins from the
'browser.search.order.REGION.INDEX' properties with values identifying
particular search plugins by name. Here, REGION is derived from a REGION for
which we have seen a region-specific default plugin.

6. Generate a JSON representation of the above information, and write the result
to browsersearch.json in the locale-specific raw resource directory
e.g. raw/browsersearch.json, raw-pt-rBR/browsersearch.json.
'''

from __future__ import (
    absolute_import,
    print_function,
    unicode_literals,
)

import argparse
import codecs
import errno
import json
import sys
import os

from mozbuild.dotproperties import (
    DotProperties,
)
from mozbuild.util import (
    FileAvoidWrite,
)
import mozpack.path as mozpath


def merge_properties(paths):
    """Merges properties from the given paths."""
    properties = DotProperties()
    for path in paths:
        try:
            properties.update(path)
        except IOError as e:
            if e.errno != errno.ENOENT:
                raise e
    return properties


def main(output, *args, **kwargs):
    parser = argparse.ArgumentParser()
    parser.add_argument('--verbose', '-v', default=False, action='store_true',
                        help='be verbose')
    parser.add_argument('--silent', '-s', default=False, action='store_true',
                        help='be silent')
    parser.add_argument('inputs', metavar='INPUT', nargs='*',
                        help='inputs, in order of priority')
    # This awkward "extra input" is an expedient work-around for an issue with
    # the build system.  It allows to specify the in-tree unlocalized version
    # of a resource where a regular input will always be the localized version.
    parser.add_argument('--fallback',
                        required=True,
                        help='fallback input')
    opts = parser.parse_args(args)

    # Ensure the fallback is valid.
    if not os.path.isfile(opts.fallback):
        print('Fallback path {fallback} is not a file!'.format(fallback=opts.fallback))
        return 1

    # Use reversed order so that the first input has higher priority to override keys.
    sources = [opts.fallback] + list(reversed(opts.inputs))
    properties = merge_properties(sources)

    # Default, not region-specific.
    default = properties.get('browser.search.defaultenginename')
    engines = properties.get_list('browser.search.order')

    # We must define at least one engine -- that's what the fallback is for.
    if not engines:
        print('No engines defined: searched in {}!'.format(sources))
        return 1

    writer = codecs.getwriter('utf-8')(sys.stdout)
    if opts.verbose:
        print('Read {len} engines: {engines}'.format(len=len(engines), engines=engines), file=writer)
        print("Default engine is '{default}'.".format(default=default), file=writer)

    browsersearch = {}
    browsersearch['default'] = default
    browsersearch['engines'] = engines

    # This gets defaults, yes; but it also gets the list of regions known.
    regions = properties.get_dict('browser.search.defaultenginename')

    browsersearch['regions'] = {}
    for region in regions.keys():
        region_default = regions[region]
        region_engines = properties.get_list('browser.search.order.{region}'.format(region=region))

        if opts.verbose:
            print("Region '{region}': Read {len} engines: {region_engines}".format(
                len=len(region_engines), region=region, region_engines=region_engines), file=writer)
            print("Region '{region}': Default engine is '{region_default}'.".format(
                region=region, region_default=region_default), file=writer)

        browsersearch['regions'][region] = {
            'default': region_default,
            'engines': region_engines,
        }

    json.dump(browsersearch, output)
    existed, updated = output.close()  # It's safe to close a FileAvoidWrite multiple times.

    if not opts.silent:
        if updated:
            print('{output} updated'.format(output=os.path.abspath(output.name)))
        else:
            print('{output} already up-to-date'.format(output=os.path.abspath(output.name)))

    return set([opts.fallback])

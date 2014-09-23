# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Script to generate the browsersearch.json file for Fennec.

This script follows these steps:

1. Read the region.properties file in all the given source directories (see
srcdir option). Merge all properties into a single dict accounting for the
priority of source directories.

2. Read the default search plugin from the 'browser.search.defaultenginename'.

3. Read the list of search plugins from the 'browser.search.order.INDEX'
properties with values identifying particular search plugins by name.

4. Generate a JSON representation of 2. and 3., and write the result to
browsersearch.json in the locale-specific raw resource directory
e.g. raw/browsersearch.json, raw-pt-rBR/browsersearch.json.
'''

from __future__ import print_function

import argparse
import json
import re
import sys
import os

from mozbuild.dotproperties import (
    DotProperties,
)
from mozbuild.util import (
    FileAvoidWrite,
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
    parser.add_argument('--srcdir', metavar='SRCDIR',
                        action='append', required=True,
                        help='directories to read inputs from, in order of priority')
    parser.add_argument('output', metavar='OUTPUT',
                        help='output')
    opts = parser.parse_args(args)

    # Use reversed order so that the first srcdir has higher priority to override keys.
    properties = merge_properties('region.properties', reversed(opts.srcdir))

    default = properties.get('browser.search.defaultenginename')
    engines = properties.get_list('browser.search.order')

    if opts.verbose:
        print('Read {len} engines: {engines}'.format(len=len(engines), engines=engines))
        print("Default engine is '{default}'.".format(default=default))

    browsersearch = {}
    browsersearch['default'] = default
    browsersearch['engines'] = engines

    # FileAvoidWrite creates its parent directories.
    output = os.path.abspath(opts.output)
    fh = FileAvoidWrite(output)
    json.dump(browsersearch, fh)
    existed, updated = fh.close()

    if not opts.silent:
        if updated:
            print('{output} updated'.format(output=output))
        else:
            print('{output} already up-to-date'.format(output=output))

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

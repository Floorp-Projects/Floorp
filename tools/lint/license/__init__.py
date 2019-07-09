# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os

from mozlint import result
from mozlint.pathutils import expand_exclusions

here = os.path.abspath(os.path.dirname(__file__))

results = []


def load_valid_license():
    """
    Load the list of license patterns
    """
    license_list = os.path.join(here, 'valid-licenses.txt')
    with open(license_list) as f:
        return f.readlines()


def is_valid_license(licenses, filename):
    """
    From a given file, check if we can find the license patterns
    in the X first lines of the file
    """
    nb_lines = 10
    with open(filename) as myfile:
        head = myfile.readlines(nb_lines)
        for l in licenses:
            if l.lower().strip() in ''.join(head).lower():
                return True
    return False


def lint(paths, config, fix=None, **lintargs):
    files = list(expand_exclusions(paths, config, lintargs['root']))

    licenses = load_valid_license()
    for f in files:
        if "/test" in f or "/gtest" in f:
            # We don't require license for test
            continue
        if not is_valid_license(licenses, f):
            res = {'path': f,
                   'message': "No matching license strings found in tools/lint/license/valid-licenses.txt",  # noqa
                   'level': 'error'
                   }
            results.append(result.from_config(config, **res))
    return results

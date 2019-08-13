# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os

from mozlint import result
from mozlint.pathutils import expand_exclusions

here = os.path.abspath(os.path.dirname(__file__))

results = []

# Official source: https://www.mozilla.org/en-US/MPL/headers/
TEMPLATES = {
    "mpl2_license":
    """
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    """.strip().splitlines(),
    "public_domain_license":
    """
    Any copyright is dedicated to the public domain.
    http://creativecommons.org/publicdomain/zero/1.0/
    """.strip().splitlines(),
}
license_list = os.path.join(here, 'valid-licenses.txt')


def load_valid_license():
    """
    Load the list of license patterns
    """
    with open(license_list) as f:
        l = f.readlines()
        # Remove the empty lines
        return filter(bool, [x.replace('\n', '') for x in l])


def is_valid_license(licenses, filename):
    """
    From a given file, check if we can find the license patterns
    in the X first lines of the file
    """
    nb_lines = 10
    with open(filename) as myfile:
        head = myfile.readlines(nb_lines)
        # Empty files don't need a license.
        if not head:
            return True

        for l in licenses:
            if l.lower().strip() in ''.join(head).lower():
                return True
    return False


def add_header(filename, header):
    """
    Add the header to the top of the file
    """
    header.append("\n")
    with open(filename, 'r+') as f:
        # lines in list format
        lines = f.readlines()

        i = 0
        if lines:
            # if the file isn't empty (__init__.py can be empty files)
            if lines[0].startswith("#!") or lines[0].startswith("<?xml "):
                i = 1

            if lines[0].startswith("/* -*- Mode"):
                i = 2
        # Insert in the top of the data structure
        lines[i:i] = header
        f.seek(0, 0)
        f.write("".join(lines))


def is_test(f):
    """
    is the file a test or not?
    """
    return ("/test" in f or "/gtest" in f or "/crashtest" in f or "/mochitest" in f
            or "/reftest" in f or "/imptest" in f or "/androidTest" in f
            or "/jit-test/" in f or "jsapi-tests/" in f)


def fix_me(filename):
    """
    Add the copyright notice to the top of the file
    """
    _, ext = os.path.splitext(filename)
    license = []

    license_template = TEMPLATES['mpl2_license']
    test = False

    if is_test(filename):
        license_template = TEMPLATES['public_domain_license']
        test = True

    if ext in ['.cpp', '.c', '.cc', '.h', '.m', '.mm', '.rs', '.js', '.jsm', '.jsx']:
        for i, l in enumerate(license_template):
            start = " "
            end = ""
            if i == 0:
                # first line, we have the /*
                start = "/"
            if i == len(license_template) - 1:
                # Last line, we end by */
                end = " */"
            license.append(start + "* " + l.strip() + end + "\n")

        add_header(filename, license)
        return

    if ext in ['.py'] or filename.endswith(".inc.xul"):
        for l in license_template:
            license.append("# " + l.strip() + "\n")
        add_header(filename, license)
        return

    if ext in ['.xml', '.xul', '.html', '.xhtml']:
        for i, l in enumerate(license_template):
            start = "   - "
            end = ""
            if i == 0:
                # first line, we have the <!--
                start = "<!-- "
            if i == 2 or (i == 1 and test):
                # Last line, we end by -->
                end = " -->"
            license.append(start + l.strip() + end + "\n")
        add_header(filename, license)
        return


def lint(paths, config, fix=None, **lintargs):
    files = list(expand_exclusions(paths, config, lintargs['root']))

    licenses = load_valid_license()

    for f in files:
        if is_test(f):
            # For now, do not do anything with test (too many)
            continue
        if not is_valid_license(licenses, f):
            res = {'path': f,
                   'message': "No matching license strings found in tools/lint/license/valid-licenses.txt",  # noqa
                   'level': 'error'
                   }
            results.append(result.from_config(config, **res))
            if fix:
                fix_me(f)
    return results

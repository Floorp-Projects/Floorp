# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function
import os
import sys
import yaml
from mozlint import result

# This simple linter checks for duplicates from
# modules/libpref/init/StaticPrefList.yaml against modules/libpref/init/all.js

# If for any reason a pref needs to appear in both files, add it to this set.
IGNORE_PREFS = {
    'devtools.console.stdout.chrome',   # Uses the 'sticky' attribute.
    'devtools.console.stdout.content',  # Uses the 'sticky' attribute.
    'fission.autostart',                # Uses the 'locked' attribute.
    'browser.dom.window.dump.enabled',  # Uses the 'sticky' attribute.
}


def get_names(pref_list_filename):
    pref_names = set()
    # We want to transform patterns like 'foo: @VAR@' into valid yaml. This
    # pattern does not happen in 'name', so it's fine to ignore these.
    # We also want to evaluate all branches of #ifdefs for pref names, so we
    # ignore anything else preprocessor related.
    file = open(pref_list_filename).read().replace('@', '')
    try:
        pref_list = yaml.safe_load(file)
    except (IOError, ValueError) as e:
        print('{}: error:\n  {}'
              .format(pref_list_filename, e), file=sys.stderr)
        sys.exit(1)

    for pref in pref_list:
        if pref['name'] not in IGNORE_PREFS:
            pref_names.add(pref['name'])
    return pref_names


# Check the names of prefs against each other, and if the pref is a duplicate
# that has not previously been noted, add that name to the list of errors.
# The checking process uses simple pattern matching rather than parsing, since
# the entries in all.js are regular enough to do this.
def check_against(js_file_to_check, pref_names):
    with open(js_file_to_check) as source:
        found_dupes = set()
        errors = []
        for lineno, line in enumerate(source, start=1):
            if 'pref(' in line:
                errors.extend(check_name_for_pref(line.strip(), pref_names, found_dupes, lineno))
        return errors


def check_name_for_pref(pref, pref_names, found_dupes, lineno):
    groups = pref.split('"')
    errors = []
    if len(groups) > 1 and '//' not in groups[0]:
        if groups[1] in pref_names and groups[1] not in found_dupes:
            found_dupes.add(groups[1])
            errors.append({
                'message': pref,
                'lineno': lineno,
                'hint': 'Remove the duplicate pref or add it to IGNORE_PREFS.',
                'level': 'error',
            })
    return errors


def checkdupes(paths, config, **kwargs):
    results = []
    topdir = os.path.join(kwargs['root'], "modules", "libpref", "init")
    pref_names = get_names(os.path.join(topdir, "StaticPrefList.yaml"))
    errors = check_against(os.path.join(topdir, "all.js"), pref_names)
    for error in errors:
        results.append(result.from_config(config, **error))
    return results

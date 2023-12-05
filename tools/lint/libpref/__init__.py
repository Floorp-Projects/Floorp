# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import sys

import yaml
from mozlint import result
from mozlint.pathutils import expand_exclusions

# This simple linter checks for duplicates from
# modules/libpref/init/StaticPrefList.yaml against modules/libpref/init/all.js

# If for any reason a pref needs to appear in both files, add it to this set.
IGNORE_PREFS = {
    "devtools.console.stdout.chrome",  # Uses the 'sticky' attribute.
    "devtools.console.stdout.content",  # Uses the 'sticky' attribute.
    "fission.autostart",  # Uses the 'locked' attribute.
    "browser.dom.window.dump.enabled",  # Uses the 'sticky' attribute.
    "apz.fling_curve_function_y2",  # This pref is a part of a series.
    "dom.postMessage.sharedArrayBuffer.bypassCOOP_COEP.insecure.enabled",  # NOQA: E501; Uses the 'locked' attribute.
    "extensions.backgroundServiceWorkerEnabled.enabled",  # NOQA: E501; Uses the 'locked' attribute.
}
PATTERN = re.compile(r"\s*pref\(\s*\"(?P<pref>.+)\"\s*,\s*(?P<val>.+)\)\s*;.*")


def get_names(pref_list_filename):
    pref_names = {}
    # We want to transform patterns like 'foo: @VAR@' into valid yaml. This
    # pattern does not happen in 'name', so it's fine to ignore these.
    # We also want to evaluate all branches of #ifdefs for pref names, so we
    # ignore anything else preprocessor related.
    file = open(pref_list_filename).read().replace("@", "")
    try:
        pref_list = yaml.safe_load(file)
    except (IOError, ValueError) as e:
        print("{}: error:\n  {}".format(pref_list_filename, e), file=sys.stderr)
        sys.exit(1)

    for pref in pref_list:
        if pref["name"] not in IGNORE_PREFS:
            pref_names[pref["name"]] = pref["value"]

    return pref_names


# Check the names of prefs against each other, and if the pref is a duplicate
# that has not previously been noted, add that name to the list of errors.
def check_against(path, pref_names):
    errors = []
    prefs = read_prefs(path)
    for pref in prefs:
        if pref["name"] in pref_names:
            errors.extend(check_value_for_pref(pref, pref_names[pref["name"]], path))
    return errors


def check_value_for_pref(some_pref, some_value, path):
    errors = []
    if some_pref["value"] == some_value:
        errors.append(
            {
                "path": path,
                "message": some_pref["raw"],
                "lineno": some_pref["line"],
                "hint": "Remove the duplicate pref or add it to IGNORE_PREFS.",
                "level": "error",
            }
        )
    return errors


# The entries in the *.js pref files are regular enough to use simple pattern
# matching to load in prefs.
def read_prefs(path):
    prefs = []
    with open(path) as source:
        for lineno, line in enumerate(source, start=1):
            match = PATTERN.match(line)
            if match:
                prefs.append(
                    {
                        "name": match.group("pref"),
                        "value": evaluate_pref(match.group("val")),
                        "line": lineno,
                        "raw": line,
                    }
                )
    return prefs


def evaluate_pref(value):
    bools = {"true": True, "false": False}
    if value in bools:
        return bools[value]
    elif value.isdigit():
        return int(value)
    return value


def checkdupes(paths, config, **kwargs):
    results = []
    errors = []
    pref_names = get_names(config["support-files"][0])
    files = list(expand_exclusions(paths, config, kwargs["root"]))
    for file in files:
        errors.extend(check_against(file, pref_names))
    for error in errors:
        results.append(result.from_config(config, **error))
    return results

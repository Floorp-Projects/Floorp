# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import difflib
import os
import sys

import yaml
from manifestparser import TestManifest
from mozlint import result
from mozlint.pathutils import expand_exclusions
from mozpack import path as mozpath

# Since this linter matches on all files with .ini extensions, we can omit
# those extensions from this allowlist, which makes it easier to match
# against variants like "mochitest-serviceworker.ini".
FILENAME_ALLOWLIST = ["mochitest", "browser", "chrome", "a11y"]

here = os.path.abspath(os.path.dirname(__file__))
ERROR_LEVEL_MANIFESTS_PATH = os.path.join(here, "error-level-manifests.yml")


def lint(paths, config, fix=None, **lintargs):
    try:
        with open(ERROR_LEVEL_MANIFESTS_PATH) as f:
            error_level_manifests = yaml.safe_load(f)
    except (IOError, ValueError) as e:
        print("{}: error:\n  {}".format(ERROR_LEVEL_MANIFESTS_PATH, e), file=sys.stderr)
        sys.exit(1)

    topsrcdir = lintargs["root"]

    results = []
    file_names = list(expand_exclusions(paths, config, lintargs["root"]))

    for file_name in file_names:
        name = os.path.basename(file_name)
        if not any(name.startswith(allowed) for allowed in FILENAME_ALLOWLIST):
            continue

        manifest = TestManifest(manifests=(file_name,), strict=False)

        test_names = [test["name"] for test in manifest.tests]
        sorted_test_names = sorted(test_names)

        if test_names != sorted_test_names:
            rel_file_path = mozpath.relpath(file_name, topsrcdir)
            level = "warning"

            if (mozpath.normsep(rel_file_path) in error_level_manifests) or (
                any(
                    mozpath.match(rel_file_path, e)
                    for e in error_level_manifests
                    if "*" in e
                )
            ):
                level = "error"

            diff_instance = difflib.Differ()
            diff_result = diff_instance.compare(test_names, sorted_test_names)
            diff_list = list(diff_result)

            res = {
                "path": rel_file_path,
                "lineno": 0,
                "column": 0,
                "message": (
                    "The mochitest test manifest is not in alphabetical order. "
                    "Expected ordering: \n\n%s\n\n" % "\n".join(sorted_test_names)
                ),
                "level": level,
                "diff": "\n".join(diff_list),
            }
            results.append(result.from_config(config, **res))

    return {"results": results, "fixed": 0}

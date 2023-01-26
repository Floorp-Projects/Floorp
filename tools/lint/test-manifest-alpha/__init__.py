# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from manifestparser import TestManifest
from mozlint import result
from mozlint.pathutils import expand_exclusions

# Since this linter matches on all files with .ini extensions, we can omit
# those extensions from this allowlist, which makes it easier to match
# against variants like "mochitest-serviceworker.ini".
FILENAME_ALLOWLIST = ["mochitest", "browser", "chrome", "a11y"]


def lint(paths, config, fix=None, **lintargs):
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
            res = {
                "path": file_name,
                "lineno": 0,
                "column": 0,
                "message": (
                    "The mochitest test manifest is not in alphabetical order. "
                    "Expected ordering: \n\n%s\n\n" % "\n".join(sorted_test_names)
                ),
                "level": "warning",
            }
            results.append(result.from_config(config, **res))

    return {"results": results, "fixed": 0}

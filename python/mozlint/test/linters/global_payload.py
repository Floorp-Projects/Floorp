# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozpack.path as mozpath
from external import external
from mozpack.files import FileFinder

from mozlint import result


def global_payload(config, **lintargs):
    # A global linter that runs the external linter to actually lint.
    finder = FileFinder(lintargs["root"])
    files = [mozpath.join(lintargs["root"], p) for p, _ in finder.find("files/**")]
    issues = external(files, config, **lintargs)
    for issue in issues:
        # Make issue look like it comes from this linter.
        issue.linter = "global_payload"
    return issues


def global_skipped(config, **lintargs):
    # A global linter that always registers a lint error.  Absence of
    # this error shows that the path exclusion mechanism can cause
    # global lint payloads to not be invoked at all.  In particular,
    # the `extensions` field means that nothing under `files/**` will
    # match.

    finder = FileFinder(lintargs["root"])
    files = [mozpath.join(lintargs["root"], p) for p, _ in finder.find("files/**")]

    issues = []
    issues.append(
        result.from_config(
            config, path=files[0], lineno=1, column=1, rule="not-skipped"
        )
    )

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import attr

from ..result import Issue


class TreeherderFormatter(object):
    """Formatter for treeherder friendly output.

    This formatter looks ugly, but prints output such that
    treeherder is able to highlight the errors and warnings.
    This is a stop-gap until bug 1276486 is fixed.
    """

    fmt = "TEST-UNEXPECTED-{level} | {path}:{lineno}{column} | {message} ({rule})"

    def __call__(self, result):
        message = []
        for path, errors in sorted(result.issues.items()):
            for err in errors:
                assert isinstance(err, Issue)

                d = attr.asdict(err)
                d["column"] = ":%s" % d["column"] if d["column"] else ""
                d["level"] = d["level"].upper()
                d["rule"] = d["rule"] or d["linter"]
                message.append(self.fmt.format(**d))

        if not message:
            message.append("No lint issues found.")
        return "\n".join(message)

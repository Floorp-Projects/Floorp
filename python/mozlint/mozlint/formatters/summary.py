# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from collections import defaultdict

import mozpack.path as mozpath

from ..util.string import pluralize


class SummaryFormatter(object):
    def __init__(self, depth=None):
        self.depth = depth or int(os.environ.get("MOZLINT_SUMMARY_DEPTH", 1))

    def __call__(self, result):
        paths = set(
            list(result.issues.keys()) + list(result.suppressed_warnings.keys())
        )

        commonprefix = mozpath.commonprefix([mozpath.abspath(p) for p in paths])
        commonprefix = commonprefix.rsplit("/", 1)[0] + "/"

        summary = defaultdict(lambda: [0, 0])
        for path in paths:
            abspath = mozpath.abspath(path)
            assert abspath.startswith(commonprefix)

            if abspath != commonprefix:
                parts = mozpath.split(mozpath.relpath(abspath, commonprefix))[
                    : self.depth
                ]
                abspath = mozpath.join(commonprefix, *parts)

            summary[abspath][0] += len(
                [r for r in result.issues[path] if r.level == "error"]
            )
            summary[abspath][1] += len(
                [r for r in result.issues[path] if r.level == "warning"]
            )
            summary[abspath][1] += result.suppressed_warnings[path]

        msg = []
        for path, (errors, warnings) in sorted(summary.items()):
            warning_str = (
                ", {}".format(pluralize("warning", warnings)) if warnings else ""
            )
            msg.append("{}: {}{}".format(path, pluralize("error", errors), warning_str))
        return "\n".join(msg)

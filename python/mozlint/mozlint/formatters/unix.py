# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os

from ..result import ResultContainer


class UnixFormatter(object):
    """Formatter that respects Unix output conventions frequently
    employed by preprocessors and compilers.  The format is
    `<FILENAME>:<LINE>[:<COL>]: <RULE> <LEVEL>: <MESSAGE>`.

    """
    fmt = "{path}:{lineno}:{column} {rule} {level}: {message}"

    def __call__(self, result, **kwargs):
        msg = []

        for path, errors in sorted(result.iteritems()):
            for err in errors:
                assert isinstance(err, ResultContainer)

                slots = {s: getattr(err, s) for s in err.__slots__}
                slots["path"] = os.path.relpath(slots["path"])
                slots["column"] = "%d:" % slots["column"] if slots["column"] else ""
                slots["rule"] = slots["rule"] or slots["linter"]

                msg.append(self.fmt.format(**slots))

        return "\n".join(msg)

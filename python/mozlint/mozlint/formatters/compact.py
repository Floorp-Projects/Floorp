# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

from ..result import Issue


class CompactFormatter(object):
    """Formatter for compact output.

    This formatter prints one error per line, mimicking the
    eslint 'compact' formatter.
    """
    # If modifying this format, please also update the vim errorformats in editor.py
    fmt = "{path}: line {lineno}{column}, {level} - {message} ({rule})"

    def __init__(self, summary=True):
        self.summary = summary

    def __call__(self, result):
        message = []
        num_problems = 0
        for path, errors in sorted(result.issues.iteritems()):
            num_problems += len(errors)
            for err in errors:
                assert isinstance(err, Issue)

                d = {s: getattr(err, s) for s in err.__slots__}
                d["column"] = ", col %s" % d["column"] if d["column"] else ""
                d['level'] = d['level'].capitalize()
                d['rule'] = d['rule'] or d['linter']
                message.append(self.fmt.format(**d))

        if self.summary and num_problems:
            message.append("\n{} problem{}".format(num_problems, '' if num_problems == 1 else 's'))
        return "\n".join(message)

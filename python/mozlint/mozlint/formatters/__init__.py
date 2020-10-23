# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json

from ..result import IssueEncoder
from .compact import CompactFormatter
from .stylish import StylishFormatter
from .summary import SummaryFormatter
from .treeherder import TreeherderFormatter
from .unix import UnixFormatter


class JSONFormatter(object):
    def __call__(self, result):
        return json.dumps(result.issues, cls=IssueEncoder)


all_formatters = {
    "compact": CompactFormatter,
    "json": JSONFormatter,
    "stylish": StylishFormatter,
    "summary": SummaryFormatter,
    "treeherder": TreeherderFormatter,
    "unix": UnixFormatter,
}


def get(name, **fmtargs):
    return all_formatters[name](**fmtargs)

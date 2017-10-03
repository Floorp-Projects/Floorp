# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import json

from ..result import ResultEncoder
from .compact import CompactFormatter
from .stylish import StylishFormatter
from .treeherder import TreeherderFormatter
from .unix import UnixFormatter


class JSONFormatter(object):
    def __call__(self, results, **kwargs):
        return json.dumps(results, cls=ResultEncoder)


all_formatters = {
    'compact': CompactFormatter,
    'json': JSONFormatter,
    'stylish': StylishFormatter,
    'treeherder': TreeherderFormatter,
    'unix': UnixFormatter,
}


def get(name, **fmtargs):
    return all_formatters[name](**fmtargs)

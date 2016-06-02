# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json

from ..result import ResultEncoder
from .stylish import StylishFormatter
from .treeherder import TreeherderFormatter


class JSONFormatter(object):
    def __call__(self, results):
        return json.dumps(results, cls=ResultEncoder)


all_formatters = {
    'json': JSONFormatter,
    'stylish': StylishFormatter,
    'treeherder': TreeherderFormatter,
}


def get(name, **fmtargs):
    return all_formatters[name](**fmtargs)

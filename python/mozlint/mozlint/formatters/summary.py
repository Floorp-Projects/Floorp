# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os
from collections import defaultdict

import mozpack.path as mozpath


class SummaryFormatter(object):

    def __init__(self, depth=None):
        self.depth = depth or int(os.environ.get('MOZLINT_SUMMARY_DEPTH', 1))

    def __call__(self, result):
        commonprefix = mozpath.commonprefix([mozpath.abspath(p) for p in result.issues])
        commonprefix = commonprefix.rsplit('/', 1)[0] + '/'

        summary = defaultdict(int)
        for path, errors in result.issues.iteritems():
            path = mozpath.abspath(path)
            assert path.startswith(commonprefix)

            if path == commonprefix:
                summary[path] += len(errors)
                continue

            parts = mozpath.split(mozpath.relpath(path, commonprefix))[:self.depth]
            path = mozpath.join(commonprefix, *parts)
            summary[path] += len(errors)

        return '\n'.join(['{}: {}'.format(k, summary[k]) for k in sorted(summary)])

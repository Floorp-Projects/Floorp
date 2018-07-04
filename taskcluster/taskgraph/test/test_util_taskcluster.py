# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import datetime
import unittest

import mozunit
from taskgraph.util.taskcluster import (
    parse_time
)


class TestTCUtils(unittest.TestCase):

    def test_parse_time(self):
        exp = datetime.datetime(2018, 10, 10, 18, 33, 3, 463000)
        assert parse_time('2018-10-10T18:33:03.463Z') == exp


if __name__ == '__main__':
    mozunit.main()

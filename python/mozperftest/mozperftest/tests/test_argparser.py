#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit
from mozperftest.argparser import PerftestArgumentParser


def test_argparser():
    parser = PerftestArgumentParser()
    args = ["test_one.js"]
    res = parser.parse_args(args)
    assert res.tests == ["test_one.js"]


if __name__ == "__main__":
    mozunit.main()

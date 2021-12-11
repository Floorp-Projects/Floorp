# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import unittest
from mozunit import main
from gecko_taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def trans1(config, tests):
    for test in tests:
        test["one"] = 1
        yield test


@transforms.add
def trans2(config, tests):
    for test in tests:
        test["two"] = 2
        yield test


class TestTransformSequence(unittest.TestCase):
    def test_sequence(self):
        tests = [{}, {"two": 1, "second": True}]
        res = list(transforms({}, tests))
        self.assertEqual(
            res,
            [
                {"two": 2, "one": 1},
                {"second": True, "two": 2, "one": 1},
            ],
        )


if __name__ == "__main__":
    main()

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import json
import os
from collections import defaultdict
from unittest import TestCase

from mozunit import main

from mozlint import ResultContainer
from mozlint import formatters


here = os.path.abspath(os.path.dirname(__file__))


class TestFormatters(TestCase):

    def __init__(self, *args, **kwargs):
        TestCase.__init__(self, *args, **kwargs)

        containers = (
            ResultContainer(
                linter='foo',
                path='a/b/c.txt',
                message="oh no foo",
                lineno=1,
            ),
            ResultContainer(
                linter='bar',
                path='d/e/f.txt',
                message="oh no bar",
                hint="try baz instead",
                level='warning',
                lineno=4,
                column=2,
                rule="bar-not-allowed",
            ),
            ResultContainer(
                linter='baz',
                path='a/b/c.txt',
                message="oh no baz",
                lineno=4,
                source="if baz:",
            ),
        )

        self.results = defaultdict(list)
        for c in containers:
            self.results[c.path].append(c)

    def test_stylish_formatter(self):
        expected = """
a/b/c.txt
  1  error  oh no foo  (foo)
  4  error  oh no baz  (baz)

d/e/f.txt
  4:2  warning  oh no bar  bar-not-allowed (bar)

\u2716 3 problems (2 errors, 1 warning)
""".strip()

        fmt = formatters.get('stylish', disable_colors=True)
        self.assertEqual(expected, fmt(self.results))

    def test_treeherder_formatter(self):
        expected = """
TEST-UNEXPECTED-ERROR | a/b/c.txt:1 | oh no foo (foo)
TEST-UNEXPECTED-ERROR | a/b/c.txt:4 | oh no baz (baz)
TEST-UNEXPECTED-WARNING | d/e/f.txt:4:2 | oh no bar (bar-not-allowed)
""".strip()

        fmt = formatters.get('treeherder')
        self.assertEqual(expected, fmt(self.results))

    def test_json_formatter(self):
        fmt = formatters.get('json')
        formatted = json.loads(fmt(self.results))

        self.assertEqual(set(formatted.keys()), set(self.results.keys()))

        slots = ResultContainer.__slots__
        for errors in formatted.values():
            for err in errors:
                self.assertTrue(all(s in err for s in slots))


if __name__ == '__main__':
    main()

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import json
import sys
from collections import defaultdict

import pytest

from mozlint import ResultContainer
from mozlint import formatters


@pytest.fixture
def results(scope='module'):
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
    results = defaultdict(list)
    for c in containers:
        results[c.path].append(c)
    return results


def test_stylish_formatter(results):
    expected = """
a/b/c.txt
  1  error  oh no foo  (foo)
  4  error  oh no baz  (baz)

d/e/f.txt
  4:2  warning  oh no bar  bar-not-allowed (bar)

\u2716 3 problems (2 errors, 1 warning)
""".strip()

    fmt = formatters.get('stylish', disable_colors=True)
    assert expected == fmt(results)


def test_treeherder_formatter(results):
    expected = """
TEST-UNEXPECTED-ERROR | a/b/c.txt:1 | oh no foo (foo)
TEST-UNEXPECTED-ERROR | a/b/c.txt:4 | oh no baz (baz)
TEST-UNEXPECTED-WARNING | d/e/f.txt:4:2 | oh no bar (bar-not-allowed)
""".strip()

    fmt = formatters.get('treeherder')
    assert expected == fmt(results)


def test_json_formatter(results):
    fmt = formatters.get('json')
    formatted = json.loads(fmt(results))

    assert set(formatted.keys()) == set(results.keys())

    slots = ResultContainer.__slots__
    for errors in formatted.values():
        for err in errors:
            assert all(s in err for s in slots)


if __name__ == '__main__':
    sys.exit(pytest.main(['--verbose', __file__]))

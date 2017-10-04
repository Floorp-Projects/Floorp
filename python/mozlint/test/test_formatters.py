# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import json
import os
from collections import defaultdict

import mozunit
import pytest

from mozlint import ResultContainer
from mozlint import formatters

NORMALISED_PATHS = {
    'abc': os.path.normpath('a/b/c.txt'),
    'def': os.path.normpath('d/e/f.txt'),
}

EXPECTED = {
    'compact': {
        'kwargs': {},
        'format': """
a/b/c.txt: line 1, Error - oh no foo (foo)
a/b/c.txt: line 4, Error - oh no baz (baz)
d/e/f.txt: line 4, col 2, Warning - oh no bar (bar-not-allowed)

3 problems
""".strip(),
    },
    'stylish': {
        'kwargs': {
            'disable_colors': True,
        },
        'format': """
a/b/c.txt
  1  error  oh no foo  (foo)
  4  error  oh no baz  (baz)

d/e/f.txt
  4:2  warning  oh no bar  bar-not-allowed (bar)

\u2716 3 problems (2 errors, 1 warning)
""".strip(),
    },
    'treeherder': {
        'kwargs': {},
        'format': """
TEST-UNEXPECTED-ERROR | a/b/c.txt:1 | oh no foo (foo)
TEST-UNEXPECTED-ERROR | a/b/c.txt:4 | oh no baz (baz)
TEST-UNEXPECTED-WARNING | d/e/f.txt:4:2 | oh no bar (bar-not-allowed)
""".strip(),
    },
    'unix': {
        'kwargs': {},
        'format': """
{abc}:1: foo error: oh no foo
{abc}:4: baz error: oh no baz
{def}:4:2: bar-not-allowed warning: oh no bar
""".format(**NORMALISED_PATHS).strip(),
    },
}


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


@pytest.mark.parametrize("name", EXPECTED.keys())
def test_formatters(results, name):
    opts = EXPECTED[name]
    fmt = formatters.get(name, **opts['kwargs'])
    assert fmt(results) == opts['format']


def test_json_formatter(results):
    fmt = formatters.get('json')
    formatted = json.loads(fmt(results))

    assert set(formatted.keys()) == set(results.keys())

    slots = ResultContainer.__slots__
    for errors in formatted.values():
        for err in errors:
            assert all(s in err for s in slots)


if __name__ == '__main__':
    mozunit.main()

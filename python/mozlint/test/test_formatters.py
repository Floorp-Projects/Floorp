# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import json
import os

import mozunit
import mozpack.path as mozpath
import pytest

from mozlint.result import Issue, ResultSummary
from mozlint import formatters

NORMALISED_PATHS = {
    'abc': os.path.normpath('a/b/c.txt'),
    'def': os.path.normpath('d/e/f.txt'),
    'cwd': mozpath.normpath(os.getcwd()),
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
    'summary': {
        'kwargs': {},
        'format': """
{cwd}/a: 2 errors
{cwd}/d: 0 errors, 1 warning
""".format(**NORMALISED_PATHS).strip(),
    },
}


@pytest.fixture
def result(scope='module'):
    containers = (
        Issue(
            linter='foo',
            path='a/b/c.txt',
            message="oh no foo",
            lineno=1,
        ),
        Issue(
            linter='bar',
            path='d/e/f.txt',
            message="oh no bar",
            hint="try baz instead",
            level='warning',
            lineno=4,
            column=2,
            rule="bar-not-allowed",
        ),
        Issue(
            linter='baz',
            path='a/b/c.txt',
            message="oh no baz",
            lineno=4,
            source="if baz:",
        ),
    )
    result = ResultSummary()
    for c in containers:
        result.issues[c.path].append(c)
    return result


@pytest.mark.parametrize("name", EXPECTED.keys())
def test_formatters(result, name):
    opts = EXPECTED[name]
    fmt = formatters.get(name, **opts['kwargs'])
    assert fmt(result) == opts['format']


def test_json_formatter(result):
    fmt = formatters.get('json')
    formatted = json.loads(fmt(result))

    assert set(formatted.keys()) == set(result.issues.keys())

    slots = Issue.__slots__
    for errors in formatted.values():
        for err in errors:
            assert all(s in err for s in slots)


if __name__ == '__main__':
    mozunit.main()

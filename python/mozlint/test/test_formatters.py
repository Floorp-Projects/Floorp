# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json

import attr
import mozpack.path as mozpath
import mozunit
import pytest

from mozlint import formatters
from mozlint.result import Issue, ResultSummary

NORMALISED_PATHS = {
    "abc": mozpath.normpath("a/b/c.txt"),
    "def": mozpath.normpath("d/e/f.txt"),
    "root": mozpath.abspath("/fake/root"),
}

EXPECTED = {
    "compact": {
        "kwargs": {},
        "format": """
/fake/root/a/b/c.txt: line 1, Error - oh no foo (foo)
/fake/root/a/b/c.txt: line 4, col 10, Error - oh no baz (baz)
/fake/root/a/b/c.txt: line 5, Error - oh no foo-diff (foo-diff)
/fake/root/d/e/f.txt: line 4, col 2, Warning - oh no bar (bar-not-allowed)

4 problems
""".strip(),
    },
    "stylish": {
        "kwargs": {"disable_colors": True},
        "format": """
/fake/root/a/b/c.txt
  1     error  oh no foo       (foo)
  4:10  error  oh no baz       (baz)
  5     error  oh no foo-diff  (foo-diff)
  diff 1
  - hello
  + hello2

/fake/root/d/e/f.txt
  4:2  warning  oh no bar  bar-not-allowed (bar)

\u2716 4 problems (3 errors, 1 warning, 0 fixed)
""".strip(),
    },
    "treeherder": {
        "kwargs": {},
        "format": """
TEST-UNEXPECTED-ERROR | /fake/root/a/b/c.txt:1 | oh no foo (foo)
TEST-UNEXPECTED-ERROR | /fake/root/a/b/c.txt:4:10 | oh no baz (baz)
TEST-UNEXPECTED-ERROR | /fake/root/a/b/c.txt:5 | oh no foo-diff (foo-diff)
TEST-UNEXPECTED-WARNING | /fake/root/d/e/f.txt:4:2 | oh no bar (bar-not-allowed)
""".strip(),
    },
    "unix": {
        "kwargs": {},
        "format": """
{abc}:1: foo error: oh no foo
{abc}:4:10: baz error: oh no baz
{abc}:5: foo-diff error: oh no foo-diff
{def}:4:2: bar-not-allowed warning: oh no bar
""".format(
            **NORMALISED_PATHS
        ).strip(),
    },
    "summary": {
        "kwargs": {},
        "format": """
{root}/a: 3 errors
{root}/d: 0 errors, 1 warning
""".format(
            **NORMALISED_PATHS
        ).strip(),
    },
}


@pytest.fixture
def result(scope="module"):
    result = ResultSummary("/fake/root")
    containers = (
        Issue(linter="foo", path="a/b/c.txt", message="oh no foo", lineno=1),
        Issue(
            linter="bar",
            path="d/e/f.txt",
            message="oh no bar",
            hint="try baz instead",
            level="warning",
            lineno="4",
            column="2",
            rule="bar-not-allowed",
        ),
        Issue(
            linter="baz",
            path="a/b/c.txt",
            message="oh no baz",
            lineno=4,
            column=10,
            source="if baz:",
        ),
        Issue(
            linter="foo-diff",
            path="a/b/c.txt",
            message="oh no foo-diff",
            lineno=5,
            source="if baz:",
            diff="diff 1\n- hello\n+ hello2",
        ),
    )
    result = ResultSummary("/fake/root")
    for c in containers:
        result.issues[c.path].append(c)
    return result


@pytest.mark.parametrize("name", EXPECTED.keys())
def test_formatters(result, name):
    opts = EXPECTED[name]
    fmt = formatters.get(name, **opts["kwargs"])
    # encoding to str bypasses a UnicodeEncodeError in pytest
    assert fmt(result) == opts["format"]


def test_json_formatter(result):
    fmt = formatters.get("json")
    formatted = json.loads(fmt(result))

    assert set(formatted.keys()) == set(result.issues.keys())

    attrs = attr.fields(Issue)
    for errors in formatted.values():
        for err in errors:
            assert all(a.name in err for a in attrs)


if __name__ == "__main__":
    mozunit.main()

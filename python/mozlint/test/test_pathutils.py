# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from fnmatch import fnmatch

import mozunit
import pytest

from mozlint import pathutils

here = os.path.abspath(os.path.dirname(__file__))
root = os.path.join(here, "filter")


def assert_paths(a, b):
    def normalize(p):
        if not os.path.isabs(p):
            p = os.path.join(root, p)
        return os.path.normpath(p)

    assert set(map(normalize, a)) == set(map(normalize, b))


@pytest.mark.parametrize(
    "test",
    (
        {
            "paths": ["a.js", "subdir1/subdir3/d.js"],
            "include": ["."],
            "exclude": ["subdir1"],
            "expected": ["a.js"],
        },
        {
            "paths": ["a.js", "subdir1/subdir3/d.js"],
            "include": ["subdir1/subdir3"],
            "exclude": ["subdir1"],
            "expected": ["subdir1/subdir3/d.js"],
        },
        {
            "paths": ["."],
            "include": ["."],
            "exclude": ["**/c.py", "subdir1/subdir3"],
            "extensions": ["py"],
            "expected": ["."],
            "expected_exclude": ["subdir2/c.py", "subdir1/subdir3"],
        },
        {
            "paths": [
                "a.py",
                "a.js",
                "subdir1/b.py",
                "subdir2/c.py",
                "subdir1/subdir3/d.py",
            ],
            "include": ["."],
            "exclude": ["**/c.py", "subdir1/subdir3"],
            "extensions": ["py"],
            "expected": ["a.py", "subdir1/b.py"],
        },
        {
            "paths": ["a.py", "a.js", "subdir2"],
            "include": ["."],
            "exclude": [],
            "extensions": ["py"],
            "expected": ["a.py", "subdir2"],
        },
        {
            "paths": ["subdir1"],
            "include": ["."],
            "exclude": ["subdir1/subdir3"],
            "extensions": ["py"],
            "expected": ["subdir1"],
            "expected_exclude": ["subdir1/subdir3"],
        },
        {
            "paths": ["docshell"],
            "include": ["docs"],
            "exclude": [],
            "expected": [],
        },
        {
            "paths": ["does/not/exist"],
            "include": ["."],
            "exclude": [],
            "expected": [],
        },
    ),
)
def test_filterpaths(test):
    expected = test.pop("expected")
    expected_exclude = test.pop("expected_exclude", [])

    paths, exclude = pathutils.filterpaths(root, **test)
    assert_paths(paths, expected)
    assert_paths(exclude, expected_exclude)


@pytest.mark.parametrize(
    "test",
    (
        {
            "paths": ["subdir1/b.js"],
            "config": {
                "exclude": ["subdir1"],
                "extensions": "js",
            },
            "expected": [],
        },
        {
            "paths": ["subdir1/subdir3"],
            "config": {
                "exclude": ["subdir1"],
                "extensions": "js",
            },
            "expected": [],
        },
    ),
)
def test_expand_exclusions(test):
    expected = test.pop("expected", [])

    paths = list(pathutils.expand_exclusions(test["paths"], test["config"], root))
    assert_paths(paths, expected)


@pytest.mark.parametrize(
    "paths,expected",
    [
        (["subdir1/*"], ["subdir1"]),
        (["subdir2/*"], ["subdir2"]),
        (["subdir1/*.*", "subdir1/subdir3/*", "subdir2/*"], ["subdir1", "subdir2"]),
        ([root + "/*", "subdir1/*.*", "subdir1/subdir3/*", "subdir2/*"], [root]),
        (["subdir1/b.py", "subdir1/subdir3"], ["subdir1/b.py", "subdir1/subdir3"]),
        (["subdir1/b.py", "subdir1/b.js"], ["subdir1/b.py", "subdir1/b.js"]),
        (["subdir1/subdir3"], ["subdir1/subdir3"]),
        (
            [
                "foo",
                "foobar",
            ],
            ["foo", "foobar"],
        ),
    ],
)
def test_collapse(paths, expected):
    os.chdir(root)

    inputs = []
    for path in paths:
        base, name = os.path.split(path)
        if "*" in name:
            for n in os.listdir(base):
                if not fnmatch(n, name):
                    continue
                inputs.append(os.path.join(base, n))
        else:
            inputs.append(path)

    print("inputs: {}".format(inputs))
    assert_paths(pathutils.collapse(inputs), expected)


if __name__ == "__main__":
    mozunit.main()

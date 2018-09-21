# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
from copy import deepcopy

import mozunit
import pytest

from mozlint import pathutils

here = os.path.abspath(os.path.dirname(__file__))
root = os.path.join(here, 'filter')


@pytest.fixture
def filterpaths():
    lintargs = {
        'root': root,
        'use_filters': True,
    }
    os.chdir(lintargs['root'])

    def inner(paths, include, exclude, extensions=None, **kwargs):
        linter = {
            'include': include,
            'exclude': exclude,
            'extensions': extensions,
        }
        largs = deepcopy(lintargs)
        largs.update(kwargs)
        return pathutils.filterpaths(paths, linter, **largs)

    return inner


def assert_paths(a, b):
    def normalize(p):
        if not os.path.isabs(p):
            p = os.path.join(root, p)
        return os.path.normpath(p)
    assert set(map(normalize, a)) == set(map(normalize, b))


def test_no_filter(filterpaths):
    args = {
        'paths': ['a.py', 'subdir1/b.py'],
        'include': ['.'],
        'exclude': ['**/*.py'],
        'use_filters': False,
    }

    paths = filterpaths(**args)
    assert_paths(paths, args['paths'])


def test_extensions(filterpaths):
    args = {
        'paths': ['a.py', 'a.js', 'subdir2'],
        'include': ['**'],
        'exclude': [],
        'extensions': ['py'],
    }

    paths = filterpaths(**args)
    assert_paths(paths, ['a.py', 'subdir2/c.py'])


TEST_CASES = (
    {
        'paths': ['a.js', 'subdir1/subdir3/d.js'],
        'include': ['.'],
        'exclude': ['subdir1'],
        'expected': ['a.js'],
    },
    {
        'paths': ['a.js', 'subdir1/subdir3/d.js'],
        'include': ['subdir1/subdir3'],
        'exclude': ['subdir1'],
        'expected': ['subdir1/subdir3/d.js'],
    },
    {
        'paths': ['.'],
        'include': ['**/*.py'],
        'exclude': ['**/c.py', 'subdir1/subdir3'],
        'expected': ['a.py', 'subdir1/b.py'],
    },
    {
        'paths': ['a.py', 'a.js', 'subdir1/b.py', 'subdir2/c.py', 'subdir1/subdir3/d.py'],
        'include': ['**/*.py'],
        'exclude': ['**/c.py', 'subdir1/subdir3'],
        'expected': ['a.py', 'subdir1/b.py'],
    },
)


@pytest.mark.parametrize('test', TEST_CASES)
def test_filterpaths(filterpaths, test):
    expected = test.pop('expected')

    paths = filterpaths(**test)
    assert_paths(paths, expected)


if __name__ == '__main__':
    mozunit.main()

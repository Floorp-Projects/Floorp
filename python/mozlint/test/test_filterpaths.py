# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import sys

import pytest

from mozlint import pathutils

here = os.path.abspath(os.path.dirname(__file__))


@pytest.fixture
def filterpaths():
    lintargs = {
        'root': os.path.join(here, 'filter'),
        'use_filters': True,
    }
    os.chdir(lintargs['root'])

    def inner(paths, include, exclude, extensions=None, **kwargs):
        linter = {
            'include': include,
            'exclude': exclude,
            'extensions': extensions,
        }
        lintargs.update(kwargs)
        return pathutils.filterpaths(paths, linter, **lintargs)

    return inner


def assert_paths(a, b):
    assert set(a) == set([os.path.normpath(t) for t in b])


def test_no_filter(filterpaths):
    args = {
        'paths': ['a.py', 'subdir1/b.py'],
        'include': ['.'],
        'exclude': ['**/*.py'],
        'use_filters': False,
    }

    paths = filterpaths(**args)
    assert set(paths) == set(args['paths'])


def test_extensions(filterpaths):
    args = {
        'paths': ['a.py', 'a.js', 'subdir2'],
        'include': ['**'],
        'exclude': [],
        'extensions': ['py'],
    }

    paths = filterpaths(**args)
    assert_paths(paths, ['a.py', 'subdir2/c.py'])


def test_include_exclude(filterpaths):
    args = {
        'paths': ['a.js', 'subdir1/subdir3/d.js'],
        'include': ['.'],
        'exclude': ['subdir1'],
    }

    paths = filterpaths(**args)
    assert_paths(paths, ['a.js'])

    args['include'] = ['subdir1/subdir3']
    paths = filterpaths(**args)
    assert_paths(paths, ['subdir1/subdir3/d.js'])

    args = {
        'paths': ['.'],
        'include': ['**/*.py'],
        'exclude': ['**/c.py', 'subdir1/subdir3'],
    }
    paths = filterpaths(**args)
    assert_paths(paths, ['a.py', 'subdir1/b.py'])

    args['paths'] = ['a.py', 'a.js', 'subdir1/b.py', 'subdir2/c.py', 'subdir1/subdir3/d.py']
    paths = filterpaths(**args)
    assert_paths(paths, ['a.py', 'subdir1/b.py'])


if __name__ == '__main__':
    sys.exit(pytest.main(['--verbose', __file__]))

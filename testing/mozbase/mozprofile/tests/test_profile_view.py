#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import mozprofile
import os
import sys
import pytest

import mozunit
from six import text_type

here = os.path.dirname(os.path.abspath(__file__))


def test_profileprint(tmpdir):
    """Test the summary function."""
    keys = set(['Files', 'Path'])

    tmpdir = tmpdir.strpath
    profile = mozprofile.FirefoxProfile(tmpdir)
    parts = profile.summary(return_parts=True)
    parts = dict(parts)

    assert parts['Path'] == tmpdir
    assert set(parts.keys()) == keys


def test_str_cast():
    """Test casting to a string."""
    profile = mozprofile.Profile()
    if sys.version_info[0] >= 3:
        assert str(profile) == profile.summary()
    else:
        assert str(profile) == profile.summary().encode("utf-8")


@pytest.mark.skipif(sys.version_info[0] >= 3,
                    reason="no unicode() operator starting from python3")
def test_unicode_cast():
    """Test casting to a unicode string."""
    profile = mozprofile.Profile()
    assert text_type(profile) == profile.summary()


def test_profile_diff():
    profile1 = mozprofile.Profile()
    profile2 = mozprofile.Profile(preferences=dict(foo='bar'))

    # diff a profile against itself; no difference
    assert mozprofile.diff(profile1, profile1) == []

    # diff two profiles
    diff = dict(mozprofile.diff(profile1, profile2))
    assert list(diff.keys()) == ['user.js']
    lines = [line.strip() for line in diff['user.js'].splitlines()]
    assert '+foo: bar' in lines

    # diff a blank vs FirefoxProfile
    ff_profile = mozprofile.FirefoxProfile()
    diff = dict(mozprofile.diff(profile2, ff_profile))
    assert list(diff.keys()) == ['user.js']
    lines = [line.strip() for line in diff['user.js'].splitlines()]
    assert '-foo: bar' in lines
    ff_pref_lines = ['+%s: %s' % (key, value)
                     for key, value in mozprofile.FirefoxProfile.preferences.items()]
    assert set(ff_pref_lines).issubset(lines)


if __name__ == '__main__':
    mozunit.main()

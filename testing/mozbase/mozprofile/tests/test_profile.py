#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os

import mozunit
import pytest

from mozprofile.prefs import Preferences
from mozprofile import (
    BaseProfile,
    Profile,
    ChromeProfile,
    FirefoxProfile,
    ThunderbirdProfile,
    create_profile,
)

here = os.path.abspath(os.path.dirname(__file__))


def test_with_profile_should_cleanup():
    with Profile() as profile:
        assert os.path.exists(profile.profile)

    # profile is cleaned
    assert not os.path.exists(profile.profile)


def test_with_profile_should_cleanup_even_on_exception():
    with pytest.raises(ZeroDivisionError):
        with Profile() as profile:
            assert os.path.exists(profile.profile)
            1 / 0  # will raise ZeroDivisionError

    # profile is cleaned
    assert not os.path.exists(profile.profile)


@pytest.mark.parametrize('app,cls', [
    ('chrome', ChromeProfile),
    ('firefox', FirefoxProfile),
    ('thunderbird', ThunderbirdProfile),
    ('unknown', None)
])
def test_create_profile(tmpdir, app, cls):
    path = tmpdir.strpath

    if cls is None:
        with pytest.raises(NotImplementedError):
            create_profile(app)
        return

    profile = create_profile(app, profile=path)
    assert isinstance(profile, BaseProfile)
    assert profile.__class__ == cls
    assert profile.profile == path


@pytest.mark.parametrize('cls', [
    Profile,
    ChromeProfile,
])
def test_merge_profile(cls):
    profile = cls(preferences={'foo': 'bar'})
    assert profile._addons == []
    assert os.path.isfile(os.path.join(profile.profile, profile.preference_file_names[0]))

    other_profile = os.path.join(here, 'files', 'dummy-profile')
    profile.merge(other_profile)

    # make sure to add a pref file for each preference_file_names in the dummy-profile
    prefs = {}
    for name in profile.preference_file_names:
        path = os.path.join(profile.profile, name)
        assert os.path.isfile(path)

        try:
            prefs.update(Preferences.read_json(path))
        except ValueError:
            prefs.update(Preferences.read_prefs(path))

    assert 'foo' in prefs
    assert len(prefs) == len(profile.preference_file_names) + 1
    assert all(name in prefs for name in profile.preference_file_names)

    assert len(profile._addons) == 1
    assert profile._addons[0].endswith('empty.xpi')
    assert os.path.exists(profile._addons[0])


if __name__ == '__main__':
    mozunit.main()

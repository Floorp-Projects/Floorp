# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import json
import os

import mozunit

from mozprofile import ChromeProfile


def test_chrome_profile_pre_existing(tmpdir):
    path = tmpdir.strpath
    profile = ChromeProfile(profile=path)
    assert not profile.create_new
    assert os.path.isdir(profile.profile)
    assert profile.profile == path


def test_chrome_profile_create_new():
    profile = ChromeProfile()
    assert profile.create_new
    assert os.path.isdir(profile.profile)
    assert profile.profile.endswith('Default')


def test_chrome_preferences(tmpdir):
    prefs = {'foo': 'bar'}
    profile = ChromeProfile(preferences=prefs)
    prefs_file = os.path.join(profile.profile, 'Preferences')

    assert os.path.isfile(prefs_file)

    with open(prefs_file) as fh:
        assert json.load(fh) == prefs

    # test with existing prefs
    prefs_file = tmpdir.join('Preferences').strpath
    with open(prefs_file, 'w') as fh:
        json.dump({'num': '1'}, fh)

    profile = ChromeProfile(profile=tmpdir.strpath, preferences=prefs)

    def assert_prefs():
        with open(prefs_file) as fh:
            data = json.load(fh)

        assert len(data) == 2
        assert data.get('foo') == 'bar'
        assert data.get('num') == '1'

    assert_prefs()
    profile.reset()
    assert_prefs()


def test_chrome_addons():
    addons = ['foo', 'bar']
    profile = ChromeProfile(addons=addons)

    assert isinstance(profile.addons, list)
    assert profile.addons == addons

    profile.addons.install('baz')
    assert profile.addons == addons + ['baz']

    profile.reset()
    assert profile.addons == addons


if __name__ == '__main__':
    mozunit.main()

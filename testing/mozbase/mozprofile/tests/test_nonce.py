#!/usr/bin/env python

"""
test nonce in prefs delimeters
see https://bugzilla.mozilla.org/show_bug.cgi?id=722804
"""

from __future__ import absolute_import

import os

import mozunit

from mozprofile.prefs import Preferences
from mozprofile.profile import Profile


def test_nonce(tmpdir):
    # make a profile with one preference
    path = tmpdir.strpath
    profile = Profile(path,
                      preferences={'foo': 'bar'},
                      restore=False)
    user_js = os.path.join(profile.profile, 'user.js')
    assert os.path.exists(user_js)

    # ensure the preference is correct
    prefs = Preferences.read_prefs(user_js)
    assert dict(prefs) == {'foo': 'bar'}

    del profile

    # augment the profile with a second preference
    profile = Profile(path,
                      preferences={'fleem': 'baz'},
                      restore=True)
    prefs = Preferences.read_prefs(user_js)
    assert dict(prefs) == {'foo': 'bar', 'fleem': 'baz'}

    # cleanup the profile;
    # this should remove the new preferences but not the old
    profile.cleanup()
    prefs = Preferences.read_prefs(user_js)
    assert dict(prefs) == {'foo': 'bar'}


if __name__ == '__main__':
    mozunit.main()

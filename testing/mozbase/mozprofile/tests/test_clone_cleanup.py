#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import mozfile

import mozunit
import pytest

from mozprofile.profile import Profile

"""
test cleanup logic for the clone functionality
see https://bugzilla.mozilla.org/show_bug.cgi?id=642843
"""


@pytest.fixture
def profile(tmpdir):
    # make a profile with one preference
    path = tmpdir.mkdtemp().strpath
    profile = Profile(path,
                      preferences={'foo': 'bar'},
                      restore=False)
    user_js = os.path.join(profile.profile, 'user.js')
    assert os.path.exists(user_js)
    return profile


def test_restore_true(profile):
    counter = [0]

    def _feedback(dir, content):
        # Called by shutil.copytree on each visited directory.
        # Used here to display info.
        #
        # Returns the items that should be ignored by
        # shutil.copytree when copying the tree, so always returns
        # an empty list.
        counter[0] += 1
        return []

    # make a clone of this profile with restore=True
    clone = Profile.clone(profile.profile, restore=True,
                          ignore=_feedback)
    try:
        clone.cleanup()

        # clone should be deleted
        assert not os.path.exists(clone.profile)
        assert counter[0] > 0
    finally:
        mozfile.remove(clone.profile)


def test_restore_false(profile):
    # make a clone of this profile with restore=False
    clone = Profile.clone(profile.profile, restore=False)
    try:
        clone.cleanup()

        # clone should still be around on the filesystem
        assert os.path.exists(clone.profile)
    finally:
        mozfile.remove(clone.profile)


def test_cleanup_on_garbage_collected(profile):
    clone = Profile.clone(profile.profile)
    profile_dir = clone.profile
    assert os.path.exists(profile_dir)
    del clone

    # clone should be deleted
    assert not os.path.exists(profile_dir)


if __name__ == '__main__':
    mozunit.main()

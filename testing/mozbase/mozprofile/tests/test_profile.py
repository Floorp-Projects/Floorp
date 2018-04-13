#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os

import mozunit
import pytest

from mozprofile import Profile


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


if __name__ == '__main__':
    mozunit.main()

#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import tempfile
from unittest import mock

import mozunit

from mozperftest.system.profile import Profile, ProfileNotFoundError
from mozperftest.tests.support import get_running_env


def test_profile():
    mach_cmd, metadata, env = get_running_env()

    with Profile(env, mach_cmd) as profile:
        profile(metadata)
        profile_dir = env.get_arg("profile-directory")
        assert os.path.exists(profile_dir)

    assert not os.path.exists(profile_dir)


CALLS = [0]


def _return_profile(*args, **kw):
    if CALLS[0] == 0:
        CALLS[0] = 1
        raise ProfileNotFoundError()

    tempdir = tempfile.mkdtemp()

    return tempdir


@mock.patch("mozperftest.system.profile.get_profile", new=_return_profile)
def test_conditionedprofile():
    mach_cmd, metadata, env = get_running_env(profile_conditioned=True)

    with Profile(env, mach_cmd) as profile:
        profile(metadata)
        profile_dir = env.get_arg("profile-directory")
        assert os.path.exists(profile_dir)

    assert not os.path.exists(profile_dir)


if __name__ == "__main__":
    mozunit.main()

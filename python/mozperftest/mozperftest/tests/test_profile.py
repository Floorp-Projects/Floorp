#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import mozunit
from mozperftest.tests.support import get_running_env
from mozperftest.browser.profile import Profile


def test_profile():
    mach_cmd, metadata, env = get_running_env()

    with Profile(env, mach_cmd) as profile:
        profile(metadata)
        profile_dir = env.get_arg("profile-directory")
        assert os.path.exists(profile_dir)

    assert not os.path.exists(profile_dir)


if __name__ == "__main__":
    mozunit.main()

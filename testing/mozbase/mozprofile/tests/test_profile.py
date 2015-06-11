#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import os
from mozprofile import Profile


class TestProfile(unittest.TestCase):
    def test_with_profile_should_cleanup(self):
        with Profile() as profile:
            self.assertTrue(os.path.exists(profile.profile))
        # profile is cleaned
        self.assertFalse(os.path.exists(profile.profile))

    def test_with_profile_should_cleanup_even_on_exception(self):
        with self.assertRaises(ZeroDivisionError):
            with Profile() as profile:
                self.assertTrue(os.path.exists(profile.profile))
                1/0  # will raise ZeroDivisionError
        # profile is cleaned
        self.assertFalse(os.path.exists(profile.profile))


if __name__ == '__main__':
    unittest.main()

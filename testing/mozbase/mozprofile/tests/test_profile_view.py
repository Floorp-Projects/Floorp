#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import mozfile
import mozprofile
import os
import tempfile
import unittest

here = os.path.dirname(os.path.abspath(__file__))

class TestProfilePrint(unittest.TestCase):

    def test_profileprint(self):
        """
        test the summary function
        """

        keys = set(['Files', 'Path', 'user.js'])
        ff_prefs = mozprofile.FirefoxProfile.preferences # shorthand
        pref_string = '\n'.join(['%s: %s' % (key, ff_prefs[key])
                                 for key in sorted(ff_prefs.keys())])

        tempdir = tempfile.mkdtemp()
        try:
            profile = mozprofile.FirefoxProfile(tempdir)
            parts = profile.summary(return_parts=True)
            parts = dict(parts)

            self.assertEqual(parts['Path'], tempdir)
            self.assertEqual(set(parts.keys()), keys)
            self.assertEqual(pref_string, parts['user.js'].strip())

        except:
            raise
        finally:
            mozfile.rmtree(tempdir)

    def test_strcast(self):
        """
        test casting to a string
        """

        profile = mozprofile.Profile()
        self.assertEqual(str(profile), profile.summary())

    def test_profile_diff(self):
        profile1 = mozprofile.Profile()
        profile2 = mozprofile.Profile(preferences=dict(foo='bar'))

        # diff a profile against itself; no difference
        self.assertEqual([], mozprofile.diff(profile1, profile1))

        # diff two profiles
        diff = dict(mozprofile.diff(profile1, profile2))
        self.assertEqual(diff.keys(), ['user.js'])
        lines = [line.strip() for line in diff['user.js'].splitlines()]
        self.assertTrue('+foo: bar' in lines)

        # diff a blank vs FirefoxProfile
        ff_profile = mozprofile.FirefoxProfile()
        diff = dict(mozprofile.diff(profile2, ff_profile))
        self.assertEqual(diff.keys(), ['user.js'])
        lines = [line.strip() for line in diff['user.js'].splitlines()]
        self.assertTrue('-foo: bar' in lines)
        ff_pref_lines = ['+%s: %s' % (key, value)
                         for key, value in mozprofile.FirefoxProfile.preferences.items()]
        self.assertTrue(set(ff_pref_lines).issubset(lines))

if __name__ == '__main__':
    unittest.main()

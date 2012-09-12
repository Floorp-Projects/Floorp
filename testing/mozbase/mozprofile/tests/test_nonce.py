#!/usr/bin/env python

"""
test nonce in prefs delimeters
see https://bugzilla.mozilla.org/show_bug.cgi?id=722804
"""

import os
import tempfile
import time
import unittest
from mozprofile.prefs import Preferences
from mozprofile.profile import Profile

class PreferencesNonceTest(unittest.TestCase):

    def test_nonce(self):

        # make a profile with one preference
        path = tempfile.mktemp()
        profile = Profile(path,
                          preferences={'foo': 'bar'},
                          restore=False)
        user_js = os.path.join(profile.profile, 'user.js')
        self.assertTrue(os.path.exists(user_js))

        # ensure the preference is correct
        prefs = Preferences.read_prefs(user_js)
        self.assertEqual(dict(prefs), {'foo': 'bar'})

        del profile

        # augment the profile with a second preference
        profile = Profile(path,
                          preferences={'fleem': 'baz'},
                          restore=True)
        prefs = Preferences.read_prefs(user_js)
        self.assertEqual(dict(prefs), {'foo': 'bar', 'fleem': 'baz'})

        # cleanup the profile;
        # this should remove the new preferences but not the old
        profile.cleanup()
        prefs = Preferences.read_prefs(user_js)
        self.assertEqual(dict(prefs), {'foo': 'bar'})

if __name__ == '__main__':
    unittest.main()

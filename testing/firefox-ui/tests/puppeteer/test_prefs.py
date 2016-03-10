# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.errors import MarionetteException

from firefox_puppeteer.testcases import FirefoxTestCase


class testPreferences(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)

        self.new_pref = 'marionette.unittest.set_pref'
        self.unknown_pref = 'marionette.unittest.unknown'

        self.bool_pref = 'browser.tabs.loadBookmarksInBackground'
        self.int_pref = 'browser.tabs.maxOpenBeforeWarn'
        self.string_pref = 'browser.newtab.url'

    def test_reset_pref(self):
        self.prefs.set_pref(self.new_pref, 'unittest')
        self.assertEqual(self.prefs.get_pref(self.new_pref), 'unittest')

        # Preference gets removed
        self.assertTrue(self.prefs.reset_pref(self.new_pref))
        self.assertEqual(self.prefs.get_pref(self.new_pref), None)

        # There is no such preference anymore
        self.assertFalse(self.prefs.reset_pref(self.new_pref))

    def test_get_pref(self):
        # check correct types
        self.assertTrue(isinstance(self.prefs.get_pref(self.bool_pref),
                                   bool))
        self.assertTrue(isinstance(self.prefs.get_pref(self.int_pref),
                                   int))
        self.assertTrue(isinstance(self.prefs.get_pref(self.string_pref),
                                   basestring))

        # unknown
        self.assertIsNone(self.prefs.get_pref(self.unknown_pref))

        # default branch
        orig_value = self.prefs.get_pref(self.int_pref)
        self.prefs.set_pref(self.int_pref, 99999)
        self.assertEqual(self.prefs.get_pref(self.int_pref), 99999)
        self.assertEqual(self.prefs.get_pref(self.int_pref, True), orig_value)

        # complex value
        properties_file = 'chrome://branding/locale/browserconfig.properties'
        self.assertEqual(self.prefs.get_pref('browser.startup.homepage'),
                         properties_file)

        value = self.prefs.get_pref('browser.startup.homepage',
                                    interface='nsIPrefLocalizedString')
        self.assertNotEqual(value, properties_file)

    def test_restore_pref(self):
        # test with single set_pref call and a new preference
        self.prefs.set_pref(self.new_pref, True)
        self.assertTrue(self.prefs.get_pref(self.new_pref))
        self.prefs.restore_pref(self.new_pref)

        orig_value = self.prefs.get_pref(self.string_pref)

        # test with single set_pref call
        self.prefs.set_pref(self.string_pref, 'unittest')
        self.assertEqual(self.prefs.get_pref(self.string_pref), 'unittest')
        self.prefs.restore_pref(self.string_pref)
        self.assertEqual(self.prefs.get_pref(self.string_pref), orig_value)

        # test with multiple set_pref calls
        self.prefs.set_pref(self.string_pref, 'unittest1')
        self.prefs.set_pref(self.string_pref, 'unittest2')
        self.assertEqual(self.prefs.get_pref(self.string_pref), 'unittest2')
        self.prefs.restore_pref(self.string_pref)
        self.assertEqual(self.prefs.get_pref(self.string_pref), orig_value)

        # test with multiple restore_pref calls
        self.prefs.set_pref(self.string_pref, 'unittest3')
        self.prefs.restore_pref(self.string_pref)
        self.assertRaises(MarionetteException,
                          self.prefs.restore_pref, self.string_pref)

        # test with an unknown pref
        self.assertRaises(MarionetteException,
                          self.prefs.restore_pref, self.unknown_pref)

    def test_restore_all_prefs(self):
        orig_bool = self.prefs.get_pref(self.bool_pref)
        orig_int = self.prefs.get_pref(self.int_pref)
        orig_string = self.prefs.get_pref(self.string_pref)

        self.prefs.set_pref(self.bool_pref, not orig_bool)
        self.prefs.set_pref(self.int_pref, 99999)
        self.prefs.set_pref(self.string_pref, 'unittest')

        self.prefs.restore_all_prefs()
        self.assertEqual(self.prefs.get_pref(self.bool_pref), orig_bool)
        self.assertEqual(self.prefs.get_pref(self.int_pref), orig_int)
        self.assertEqual(self.prefs.get_pref(self.string_pref), orig_string)

    def test_set_pref_casted_values(self):
        # basestring as boolean
        self.prefs.set_pref(self.bool_pref, '')
        self.assertFalse(self.prefs.get_pref(self.bool_pref))

        self.prefs.set_pref(self.bool_pref, 'unittest')
        self.assertTrue(self.prefs.get_pref(self.bool_pref))

        # int as boolean
        self.prefs.set_pref(self.bool_pref, 0)
        self.assertFalse(self.prefs.get_pref(self.bool_pref))

        self.prefs.set_pref(self.bool_pref, 5)
        self.assertTrue(self.prefs.get_pref(self.bool_pref))

        # boolean as int
        self.prefs.set_pref(self.int_pref, False)
        self.assertEqual(self.prefs.get_pref(self.int_pref), 0)

        self.prefs.set_pref(self.int_pref, True)
        self.assertEqual(self.prefs.get_pref(self.int_pref), 1)

        # int as string
        self.prefs.set_pref(self.string_pref, 54)
        self.assertEqual(self.prefs.get_pref(self.string_pref), '54')

    def test_set_pref_invalid(self):
        self.assertRaises(AssertionError,
                          self.prefs.set_pref, self.new_pref, None)

    def test_set_pref_new_preference(self):
        self.prefs.set_pref(self.new_pref, True)
        self.assertTrue(self.prefs.get_pref(self.new_pref))
        self.prefs.restore_pref(self.new_pref)

        self.prefs.set_pref(self.new_pref, 5)
        self.assertEqual(self.prefs.get_pref(self.new_pref), 5)
        self.prefs.restore_pref(self.new_pref)

        self.prefs.set_pref(self.new_pref, 'test')
        self.assertEqual(self.prefs.get_pref(self.new_pref), 'test')
        self.prefs.restore_pref(self.new_pref)

    def test_set_pref_new_values(self):
        self.prefs.set_pref(self.bool_pref, True)
        self.assertTrue(self.prefs.get_pref(self.bool_pref))

        self.prefs.set_pref(self.int_pref, 99999)
        self.assertEqual(self.prefs.get_pref(self.int_pref), 99999)

        self.prefs.set_pref(self.string_pref, 'test_string')
        self.assertEqual(self.prefs.get_pref(self.string_pref), 'test_string')

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.errors import JavascriptException


class TestPreferences(MarionetteTestCase):
    prefs = {
        "bool": "marionette.test.bool",
        "int": "marionette.test.int",
        "string": "marionette.test.string",
    }

    def tearDown(self):
        for pref in self.prefs.values():
            self.marionette.clear_pref(pref)

        super(TestPreferences, self).tearDown()

    def test_clear_pref(self):
        self.assertIsNone(self.marionette.get_pref(self.prefs["bool"]))

        self.marionette.set_pref(self.prefs["bool"], True)
        self.assertTrue(self.marionette.get_pref(self.prefs["bool"]))

        self.marionette.clear_pref(self.prefs["bool"])
        self.assertIsNone(self.marionette.get_pref(self.prefs["bool"]))

    def test_get_and_set_pref(self):
        # By default none of the preferences are set
        self.assertIsNone(self.marionette.get_pref(self.prefs["bool"]))
        self.assertIsNone(self.marionette.get_pref(self.prefs["int"]))
        self.assertIsNone(self.marionette.get_pref(self.prefs["string"]))

        # Test boolean values
        self.marionette.set_pref(self.prefs["bool"], True)
        value = self.marionette.get_pref(self.prefs["bool"])
        self.assertTrue(value)
        self.assertEqual(type(value), bool)

        # Test int values
        self.marionette.set_pref(self.prefs["int"], 42)
        value = self.marionette.get_pref(self.prefs["int"])
        self.assertEqual(value, 42)
        self.assertEqual(type(value), int)

        # Test string values
        self.marionette.set_pref(self.prefs["string"], "abc")
        value = self.marionette.get_pref(self.prefs["string"])
        self.assertEqual(value, "abc")
        self.assertTrue(isinstance(value, basestring))

        # Test reset value
        self.marionette.set_pref(self.prefs["string"], None)
        self.assertIsNone(self.marionette.get_pref(self.prefs["string"]))

    def test_get_set_pref_default_branch(self):
        pref_default = "marionette.test.pref_default1"
        self.assertIsNone(self.marionette.get_pref(self.prefs["string"]))

        self.marionette.set_pref(pref_default, "default_value", default_branch=True)
        self.assertEqual(self.marionette.get_pref(pref_default), "default_value")
        self.assertEqual(self.marionette.get_pref(pref_default, default_branch=True),
                         "default_value")

        self.marionette.set_pref(pref_default, "user_value")
        self.assertEqual(self.marionette.get_pref(pref_default), "user_value")
        self.assertEqual(self.marionette.get_pref(pref_default, default_branch=True),
                         "default_value")

        self.marionette.clear_pref(pref_default)
        self.assertEqual(self.marionette.get_pref(pref_default), "default_value")

    def test_get_pref_value_type(self):
        # Without a given value type the properties URL will be returned only
        pref_complex = "browser.startup.homepage"
        properties_file = "chrome://branding/locale/browserconfig.properties"
        self.assertEqual(self.marionette.get_pref(pref_complex, default_branch=True),
                         properties_file)

        # Otherwise the property named like the pref will be translated
        value = self.marionette.get_pref(pref_complex, default_branch=True,
                                         value_type="nsIPrefLocalizedString")
        self.assertNotEqual(value, properties_file)

    def test_set_prefs(self):
        # By default none of the preferences are set
        self.assertIsNone(self.marionette.get_pref(self.prefs["bool"]))
        self.assertIsNone(self.marionette.get_pref(self.prefs["int"]))
        self.assertIsNone(self.marionette.get_pref(self.prefs["string"]))

        # Set a value on the default branch first
        pref_default = "marionette.test.pref_default2"
        self.assertIsNone(self.marionette.get_pref(pref_default))
        self.marionette.set_prefs({pref_default: "default_value"}, default_branch=True)

        # Set user values
        prefs = {self.prefs["bool"]: True, self.prefs["int"]: 42,
                 self.prefs["string"]: "abc", pref_default: "user_value"}
        self.marionette.set_prefs(prefs)

        self.assertTrue(self.marionette.get_pref(self.prefs["bool"]))
        self.assertEqual(self.marionette.get_pref(self.prefs["int"]), 42)
        self.assertEqual(self.marionette.get_pref(self.prefs["string"]), "abc")
        self.assertEqual(self.marionette.get_pref(pref_default), "user_value")
        self.assertEqual(self.marionette.get_pref(pref_default, default_branch=True),
                         "default_value")

    def test_using_prefs(self):
        # Test that multiple preferences can be set with "using_prefs", and that
        # they are set correctly and unset correctly after leaving the context
        # manager.
        pref_not_existent = "marionette.test.not_existent1"
        pref_default = "marionette.test.pref_default3"

        self.marionette.set_prefs({self.prefs["string"]: "abc",
                                   self.prefs["int"]: 42,
                                   self.prefs["bool"]: False,
                                   })
        self.assertFalse(self.marionette.get_pref(self.prefs["bool"]))
        self.assertEqual(self.marionette.get_pref(self.prefs["int"]), 42)
        self.assertEqual(self.marionette.get_pref(self.prefs["string"]), "abc")
        self.assertIsNone(self.marionette.get_pref(pref_not_existent))

        with self.marionette.using_prefs({self.prefs["bool"]: True,
                                          self.prefs["int"]: 24,
                                          self.prefs["string"]: "def",
                                          pref_not_existent: "existent"}):

            self.assertTrue(self.marionette.get_pref(self.prefs["bool"]), True)
            self.assertEquals(self.marionette.get_pref(self.prefs["int"]), 24)
            self.assertEquals(self.marionette.get_pref(self.prefs["string"]), "def")
            self.assertEquals(self.marionette.get_pref(pref_not_existent), "existent")

        self.assertFalse(self.marionette.get_pref(self.prefs["bool"]))
        self.assertEqual(self.marionette.get_pref(self.prefs["int"]), 42)
        self.assertEqual(self.marionette.get_pref(self.prefs["string"]), "abc")
        self.assertIsNone(self.marionette.get_pref(pref_not_existent))

        # Using context with default branch
        self.marionette.set_pref(pref_default, "default_value", default_branch=True)
        self.assertEqual(self.marionette.get_pref(pref_default, default_branch=True),
                         "default_value")

        with self.marionette.using_prefs({pref_default: "new_value"}, default_branch=True):
            self.assertEqual(self.marionette.get_pref(pref_default, default_branch=True),
                             "new_value")

        self.assertEqual(self.marionette.get_pref(pref_default, default_branch=True),
                         "default_value")

    def test_using_prefs_exception(self):
        # Test that throwing an exception inside the context manager doesn"t
        # prevent the preferences from being restored at context manager exit.
        self.marionette.set_pref(self.prefs["string"], "abc")

        try:
            with self.marionette.using_prefs({self.prefs["string"]: "def"}):
                self.assertEquals(self.marionette.get_pref(self.prefs["string"]), "def")
                self.marionette.execute_script("return foo.bar.baz;")
        except JavascriptException:
            pass

        self.assertEquals(self.marionette.get_pref(self.prefs["string"]), "abc")

# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.errors import JavascriptException


class TestUsingPrefs(MarionetteTestCase):
    string_pref = 'marionette.test.string'
    int_pref = 'marionette.test.int'
    bool_pref = 'marionette.test.bool'
    non_existent_pref = 'marionette.test.non_existent.string'

    def test_using_prefs(self):
        # Test that multiple preferences can be set with 'using_prefs', and that
        # they are set correctly and unset correctly after leaving the context
        # manager.
        self.marionette.set_prefs({self.string_pref: 'old',
                                   self.int_pref: 42,
                                   self.bool_pref: False})

        with self.marionette.using_prefs({self.string_pref: 'new',
                                          self.int_pref: 10 ** 9,
                                          self.bool_pref: True,
                                          self.non_existent_pref: 'existent'}):
            self.assertEquals(self.marionette.get_pref(self.string_pref), 'new')
            self.assertEquals(self.marionette.get_pref(self.int_pref), 10 ** 9)
            self.assertEquals(self.marionette.get_pref(self.bool_pref), True)
            self.assertEquals(self.marionette.get_pref(self.non_existent_pref),
                              'existent')

        self.assertEquals(self.marionette.get_pref(self.string_pref), 'old')
        self.assertEquals(self.marionette.get_pref(self.int_pref), 42)
        self.assertEquals(self.marionette.get_pref(self.bool_pref), False)
        self.assertEquals(self.marionette.get_pref(self.non_existent_pref), None)

    def test_exception_using_prefs(self):
        # Test that throwing an exception inside the context manager doesn't
        # prevent the preferences from being restored at context manager exit.
        self.marionette.set_pref(self.string_pref, 'old')

        with self.marionette.using_prefs({self.string_pref: 'new'}):
            self.assertEquals(self.marionette.get_pref(self.string_pref), 'new')
            self.assertRaises(JavascriptException,
                              self.marionette.execute_script,
                              "return foo.bar.baz;")

        self.assertEquals(self.marionette.get_pref(self.string_pref), 'old')

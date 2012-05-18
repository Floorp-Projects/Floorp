# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from errors import JavascriptException, MarionetteException

class TestSpecialPowers(MarionetteTestCase):

    def test_prefs(self):
        self.marionette.set_context("chrome")
        result = self.marionette.execute_script("""
        SpecialPowers.setCharPref("testing.marionette.charpref", "blabla");
        return SpecialPowers.getCharPref("testing.marionette.charpref")
        """);
        self.assertEqual(result, "blabla")

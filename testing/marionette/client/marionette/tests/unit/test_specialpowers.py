# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from errors import JavascriptException, MarionetteException

class TestSpecialPowersContent(MarionetteTestCase):

    testpref = "testing.marionette.contentcharpref"
    testvalue = "blabla"

    def test_prefs(self):
        result = self.marionette.execute_script("""
        SpecialPowers.setCharPref("%(pref)s", "%(value)s");
        return SpecialPowers.getCharPref("%(pref)s")
        """ % {'pref': self.testpref, 'value': self.testvalue});
        self.assertEqual(result, self.testvalue)

    def test_prefs_after_navigate(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.test_prefs()

class TestSpecialPowersChrome(TestSpecialPowersContent):

    testpref = "testing.marionette.chromecharpref"
    testvalue = "blabla"

    def setUp(self):
        super(TestSpecialPowersChrome, self).setUp()
        self.marionette.set_context("chrome")

    def test_prefs_after_navigate(self):
        pass

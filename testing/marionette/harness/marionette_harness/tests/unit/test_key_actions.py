# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from six.moves.urllib.parse import quote

from marionette_driver.by import By
from marionette_driver.keys import Keys
from marionette_harness import MarionetteTestCase, WindowManagerMixin


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


class TestKeyActions(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestKeyActions, self).setUp()
        self.key_chain = self.marionette.actions.sequence("key", "keyboard_id")

        if self.marionette.session_capabilities["platformName"] == "mac":
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL

        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)
        self.reporter_element = self.marionette.find_element(By.ID, "keyReporter")
        self.reporter_element.click()

    def tearDown(self):
        self.marionette.actions.release()

        super(TestKeyActions, self).tearDown()

    @property
    def key_reporter_value(self):
        return self.reporter_element.get_property("value")

    def test_basic_input(self):
        self.key_chain.key_down("a").key_down("b").key_down("c").perform()
        self.assertEqual(self.key_reporter_value, "abc")

    def test_upcase_input(self):
        self.key_chain.key_down(Keys.SHIFT).key_down("a").key_up(Keys.SHIFT).key_down(
            "b"
        ).key_down("c").perform()
        self.assertEqual(self.key_reporter_value, "Abc")

    def test_replace_input(self):
        self.key_chain.key_down("a").key_down("b").key_down("c").perform()
        self.assertEqual(self.key_reporter_value, "abc")

        self.key_chain.key_down(self.mod_key).key_down("a").key_up(
            self.mod_key
        ).key_down("x").perform()
        self.assertEqual(self.key_reporter_value, "x")

    def test_clear_input(self):
        self.key_chain.key_down("a").key_down("b").key_down("c").perform()
        self.assertEqual(self.key_reporter_value, "abc")

        self.key_chain.key_down(self.mod_key).key_down("a").key_down("x").perform()
        self.assertEqual(self.key_reporter_value, "")

    def test_input_with_wait(self):
        self.key_chain.key_down("a").key_down("b").key_down("c").perform()
        self.key_chain.key_down(self.mod_key).key_down("a").pause(250).key_down(
            "x"
        ).perform()
        self.assertEqual(self.key_reporter_value, "")

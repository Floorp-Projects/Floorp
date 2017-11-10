# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import urllib

from marionette_driver.by import By
from marionette_driver.errors import ElementNotInteractableException
from marionette_driver.keys import Keys

from marionette_harness import MarionetteTestCase, skip, skip_if_mobile


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(urllib.quote(doc))


class TypingTestCase(MarionetteTestCase):

    def setUp(self):
        super(TypingTestCase, self).setUp()

        if self.marionette.session_capabilities["platformName"] == "darwin":
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL


class TestTypingChrome(TypingTestCase):

    def setUp(self):
        super(TestTypingChrome, self).setUp()
        self.marionette.set_context("chrome")

    @skip_if_mobile("Interacting with chrome elements not available for Fennec")
    def test_cut_and_paste_shortcuts(self):
        with self.marionette.using_context("content"):
            test_html = self.marionette.absolute_url("keyboard.html")
            self.marionette.navigate(test_html)

            key_reporter = self.marionette.find_element(By.ID, "keyReporter")
            self.assertEqual("", key_reporter.get_property("value"))
            key_reporter.send_keys("zyxwvutsr")
            self.assertEqual("zyxwvutsr", key_reporter.get_property("value"))

            # select all and cut
            key_reporter.send_keys(self.mod_key, "a")
            key_reporter.send_keys(self.mod_key, "x")
            self.assertEqual("", key_reporter.get_property("value"))

        url_bar = self.marionette.find_element(By.ID, "urlbar")

        # Clear contents first
        url_bar.send_keys(self.mod_key, "a")
        url_bar.send_keys(Keys.BACK_SPACE)
        self.assertEqual("", url_bar.get_attribute("value"))

        url_bar.send_keys(self.mod_key, "v")
        self.assertEqual("zyxwvutsr", url_bar.get_property("value"))


class TestTypingContent(TypingTestCase):

    def test_should_fire_key_press_events(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)
        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("a")
        result = self.marionette.find_element(By.ID, "result")
        self.assertTrue("press:" in result.text)

    def test_should_fire_key_down_events(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)
        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("I")
        result = self.marionette.find_element(By.ID, "result")
        self.assertTrue("down" in result.text)

    def test_should_fire_key_up_events(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("a")
        result = self.marionette.find_element(By.ID, "result")
        self.assertTrue("up:" in result.text)

    def test_should_type_lowercase_characters(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("abc def")
        self.assertEqual("abc def", key_reporter.get_property("value"))

    def test_should_type_uppercase_characters(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("ABC DEF")
        self.assertEqual("ABC DEF", key_reporter.get_property("value"))

    def test_cut_and_paste_shortcuts(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        self.assertEqual("", key_reporter.get_property("value"))
        key_reporter.send_keys("zyxwvutsr")
        self.assertEqual("zyxwvutsr", key_reporter.get_property("value"))

        # select all and cut
        key_reporter.send_keys(self.mod_key, "a")
        key_reporter.send_keys(self.mod_key, "x")
        self.assertEqual("", key_reporter.get_property("value"))

        key_reporter.send_keys(self.mod_key, "v")
        self.assertEqual("zyxwvutsr", key_reporter.get_property("value"))

    def test_should_type_a_quote_characters(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("\"")
        self.assertEqual("\"", key_reporter.get_property("value"))

    def test_should_type_an_at_character(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("@")
        self.assertEqual("@", key_reporter.get_property("value"))

    def test_should_type_a_mix_of_upper_and_lower_case_character(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("me@eXample.com")
        self.assertEqual("me@eXample.com", key_reporter.get_property("value"))

    def test_arrow_keys_are_not_printable(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys(Keys.ARROW_LEFT)
        self.assertEqual("", key_reporter.get_property("value"))

    def test_will_simulate_a_key_up_when_entering_text_into_input_elements(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyUp")
        element.send_keys("I like cheese")
        result = self.marionette.find_element(By.ID, "result")
        self.assertEqual(result.text, "I like cheese")

    def test_will_simulate_a_key_down_when_entering_text_into_input_elements(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyDown")
        element.send_keys("I like cheese")
        result = self.marionette.find_element(By.ID, "result")
        #  Because the key down gets the result before the input element is
        #  filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def test_will_simulate_a_key_press_when_entering_text_into_input_elements(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyPress")
        element.send_keys("I like cheese")
        result = self.marionette.find_element(By.ID, "result")
        #  Because the key down gets the result before the input element is
        #  filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def test_will_simulate_a_keyup_when_entering_text_into_textareas(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyUpArea")
        element.send_keys("I like cheese")
        result = self.marionette.find_element(By.ID, "result")
        self.assertEqual("I like cheese", result.text)

    def test_will_simulate_a_keydown_when_entering_text_into_textareas(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyDownArea")
        element.send_keys("I like cheese")
        result = self.marionette.find_element(By.ID, "result")
        #  Because the key down gets the result before the input element is
        #  filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def test_will_simulate_a_keypress_when_entering_text_into_textareas(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyPressArea")
        element.send_keys("I like cheese")
        result = self.marionette.find_element(By.ID, "result")
        #  Because the key down gets the result before the input element is
        #  filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    @skip_if_mobile("Bug 1333069 - Assertion: 'down: 40' not found in u''")
    def test_should_report_key_code_of_arrow_keys_up_down_events(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        result = self.marionette.find_element(By.ID, "result")
        element = self.marionette.find_element(By.ID, "keyReporter")

        element.send_keys(Keys.ARROW_DOWN)

        self.assertIn("down: 40", result.text.strip())
        self.assertIn("up: 40", result.text.strip())

        element.send_keys(Keys.ARROW_UP)
        self.assertIn("down: 38", result.text.strip())
        self.assertIn("up: 38", result.text.strip())

        element.send_keys(Keys.ARROW_LEFT)
        self.assertIn("down: 37", result.text.strip())
        self.assertIn("up: 37", result.text.strip())

        element.send_keys(Keys.ARROW_RIGHT)
        self.assertIn("down: 39", result.text.strip())
        self.assertIn("up: 39", result.text.strip())

        #  And leave no rubbish/printable keys in the "keyReporter"
        self.assertEqual("", element.get_property("value"))

    @skip("Reenable in Bug 1068728")
    def test_numeric_shift_keys(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        result = self.marionette.find_element(By.ID, "result")
        element = self.marionette.find_element(By.ID, "keyReporter")
        numeric_shifts_etc = "~!@#$%^&*()_+{}:i\"<>?|END~"
        element.send_keys(numeric_shifts_etc)
        self.assertEqual(numeric_shifts_etc, element.get_property("value"))
        self.assertIn(" up: 16", result.text.strip())

    def test_numeric_non_shift_keys(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID, "keyReporter")
        numeric_line_chars_non_shifted = "`1234567890-=[]\\,.'/42"
        element.send_keys(numeric_line_chars_non_shifted)
        self.assertEqual(numeric_line_chars_non_shifted, element.get_property("value"))

    def test_lowercase_alpha_keys(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyReporter")
        lower_alphas = "abcdefghijklmnopqrstuvwxyz"
        element.send_keys(lower_alphas)
        self.assertEqual(lower_alphas, element.get_property("value"))

    @skip("Reenable in Bug 1068735")
    def test_uppercase_alpha_keys(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        result = self.marionette.find_element(By.ID, "result")
        element = self.marionette.find_element(By.ID, "keyReporter")
        upper_alphas = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        element.send_keys(upper_alphas)
        self.assertEqual(upper_alphas, element.get_property("value"))
        self.assertIn(" up: 16", result.text.strip())

    @skip("Reenable in Bug 1068726")
    def test_all_printable_keys(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        result = self.marionette.find_element(By.ID, "result")
        element = self.marionette.find_element(By.ID, "keyReporter")
        all_printable = ("!\"#$%&'()*+,-./0123456789:<=>?@ "
                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ [\\]^_`"
                         "abcdefghijklmnopqrstuvwxyz{|}~")
        element.send_keys(all_printable)

        self.assertTrue(all_printable, element.get_property("value"))
        self.assertIn(" up: 16", result.text.strip())

    @skip("Reenable in Bug 1068733")
    def test_special_space_keys(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyReporter")
        element.send_keys("abcd" + Keys.SPACE + "fgh" + Keys.SPACE + "ij")
        self.assertEqual("abcd fgh ij", element.get_property("value"))

    def test_should_type_an_integer(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyReporter")
        element.send_keys(1234)
        self.assertEqual("1234", element.get_property("value"))

    def test_should_send_keys_to_elements_without_the_value_attribute(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        # If we don't get an error below we are good
        self.marionette.find_element(By.TAG_NAME, "body").send_keys("foo")

    def test_not_interactable_if_hidden(self):
        test_html = self.marionette.absolute_url("keyboard.html")
        self.marionette.navigate(test_html)

        not_displayed = self.marionette.find_element(By.ID, "notDisplayed")
        self.assertRaises(ElementNotInteractableException, not_displayed.send_keys, "foo")

    def test_appends_to_input_text(self):
        self.marionette.navigate(inline("<input>"))
        el = self.marionette.find_element(By.TAG_NAME, "input")
        el.send_keys("foo")
        el.send_keys("bar")
        self.assertEqual("foobar", el.get_property("value"))

    def test_appends_to_textarea(self):
        self.marionette.navigate(inline("<textarea></textarea>"))
        textarea = self.marionette.find_element(By.TAG_NAME, "textarea")
        textarea.send_keys("foo")
        textarea.send_keys("bar")
        self.assertEqual("foobar", textarea.get_property("value"))

    def test_send_keys_to_type_input(self):
        test_html = self.marionette.absolute_url("html5/test_html_inputs.html")
        self.marionette.navigate(test_html)

        num_input = self.marionette.find_element(By.ID, 'number')
        self.assertEqual("",
                         self.marionette.execute_script("return arguments[0].value", [num_input]))
        num_input.send_keys("1234")
        self.assertEqual('1234',
                         self.marionette.execute_script("return arguments[0].value", [num_input]))

    def test_insert_keys(self):
        l = self.marionette.find_element(By.ID, "change")
        l.send_keys("abde")
        self.assertEqual("abde", self.marionette.execute_script("return arguments[0].value;", [l]))

        # Set caret position to the middle of the input text.
        self.marionette.execute_script(
            """var el = arguments[0];
            el.selectionStart = el.selectionEnd = el.value.length / 2;""",
            script_args=[l])

        l.send_keys("c")
        self.assertEqual("abcde",
                         self.marionette.execute_script("return arguments[0].value;", [l]))

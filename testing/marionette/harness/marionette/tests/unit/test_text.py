# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.keys import Keys
from marionette_driver.by import By


class TestText(MarionetteTestCase):
    def test_getText(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element(By.ID, "mozLink")
        self.assertEqual("Click me!", l.text)

    def test_clearText(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element(By.NAME, "myInput")
        self.assertEqual("asdf", self.marionette.execute_script("return arguments[0].value;", [l]))
        l.clear()
        self.assertEqual("", self.marionette.execute_script("return arguments[0].value;", [l]))

    def test_sendKeys(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element(By.NAME, "myInput")
        self.assertEqual("asdf", self.marionette.execute_script("return arguments[0].value;", [l]))

        # Set caret position to the middle of the input text.
        self.marionette.execute_script(
            """var el = arguments[0];
            el.selectionStart = el.selectionEnd = el.value.length / 2;""",
            script_args=[l])

        l.send_keys("o")
        self.assertEqual("asodf", self.marionette.execute_script("return arguments[0].value;", [l]))

    def test_send_keys_to_type_input(self):
        test_html = self.marionette.absolute_url("html5/test_html_inputs.html")
        self.marionette.navigate(test_html)
        num_input = self.marionette.find_element(By.ID, 'number')
        self.assertEqual("", self.marionette.execute_script("return arguments[0].value", [num_input]))
        num_input.send_keys("1234")
        self.assertEqual('1234', self.marionette.execute_script("return arguments[0].value", [num_input]))

    def test_should_fire_key_press_events(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("a")

        result = self.marionette.find_element(By.ID, "result")
        self.assertTrue("press:" in result.text)

    def test_should_fire_key_down_events(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("a")

        result = self.marionette.find_element(By.ID, "result")
        self.assertTrue("down:" in result.text)

    def test_should_fire_key_up_events(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("a")

        result = self.marionette.find_element(By.ID, "result")
        self.assertTrue("up:" in result.text)

    def test_should_type_lowercase_characters(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("abc def")

        self.assertEqual("abc def", key_reporter.get_property("value"))

    def test_should_type_uppercase_characters(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("ABC DEF")

        self.assertEqual("ABC DEF", key_reporter.get_property("value"))

    def test_should_type_a_quote_characters(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys('"')

        self.assertEqual('"', key_reporter.get_property("value"))

    def test_should_type_an_at_character(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys('@')

        self.assertEqual("@", key_reporter.get_property("value"))

    def test_should_type_a_mix_of_upper_and_lower_case_character(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys("me@EXampLe.com")

        self.assertEqual("me@EXampLe.com", key_reporter.get_property("value"))

    def test_arrow_keys_are_not_printable(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        key_reporter = self.marionette.find_element(By.ID, "keyReporter")
        key_reporter.send_keys(Keys.ARROW_LEFT)

        self.assertEqual("", key_reporter.get_property("value"))

    def test_will_simulate_a_key_up_when_entering_text_into_input_elements(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID, "keyUp")
        element.send_keys("I like cheese")

        result = self.marionette.find_element(By.ID, "result")
        self.assertEqual(result.text, "I like cheese")

    def test_will_simulate_a_key_down_when_entering_text_into_input_elements(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID, "keyDown")
        element.send_keys("I like cheese")

        result = self.marionette.find_element(By.ID, "result")
        # Because the key down gets the result before the input element is
        # filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def test_will_simulate_a_key_press_when_entering_text_into_input_elements(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID, "keyPress")
        element.send_keys("I like cheese")

        result = self.marionette.find_element(By.ID, "result")
        # Because the key down gets the result before the input element is
        # filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def test_will_simulate_a_keyup_when_entering_text_into_textareas(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID, "keyUpArea")
        element.send_keys("I like cheese")

        result = self.marionette.find_element(By.ID, "result")
        self.assertEqual(result.text, "I like cheese")

    def test_will_simulate_a_keydown_when_entering_text_into_textareas(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID, "keyDownArea")
        element.send_keys("I like cheese")

        result = self.marionette.find_element(By.ID, "result")
        # Because the key down gets the result before the input element is
        # filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def test_will_simulate_a_keypress_when_entering_text_into_textareas(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID, "keyPressArea")
        element.send_keys("I like cheese")

        result = self.marionette.find_element(By.ID, "result")
        # Because the key down gets the result before the input element is
        # filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def test_should_report_key_code_of_arrow_keys_up_down_events(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        result = self.marionette.find_element(By.ID, "result")
        element = self.marionette.find_element(By.ID, "keyReporter")
        element.send_keys(Keys.ARROW_DOWN)
        self.assertTrue("down: 40" in result.text.strip())
        self.assertTrue("up: 40" in result.text.strip())

        element.send_keys(Keys.ARROW_UP)
        self.assertTrue("down: 38" in result.text.strip())
        self.assertTrue("up: 38" in result.text.strip())

        element.send_keys(Keys.ARROW_LEFT)
        self.assertTrue("down: 37" in result.text.strip())
        self.assertTrue("up: 37" in result.text.strip())

        element.send_keys(Keys.ARROW_RIGHT)
        self.assertTrue("down: 39" in result.text.strip())
        self.assertTrue("up: 39" in result.text.strip())

        #  And leave no rubbish/printable keys in the "keyReporter"
        self.assertEqual("", element.get_property("value"))

    def testNumericNonShiftKeys(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID, "keyReporter")
        numericLineCharsNonShifted = "`1234567890-=[]\\,.'/42"
        element.send_keys(numericLineCharsNonShifted)
        self.assertEqual(numericLineCharsNonShifted, element.get_property("value"))

    def testShouldTypeAnInteger(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID, "keyReporter")
        element.send_keys(1234)
        self.assertEqual("1234", element.get_property("value"))

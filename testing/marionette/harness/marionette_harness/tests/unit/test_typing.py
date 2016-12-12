# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import urllib

from marionette_driver.by import By
from marionette_driver.errors import ElementNotVisibleException
from marionette_driver.keys import Keys

from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(urllib.quote(doc))


class TestTyping(MarionetteTestCase):
    def testShouldFireKeyPressEvents(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        keyReporter = self.marionette.find_element(By.ID, "keyReporter")
        keyReporter.send_keys("a")
        result = self.marionette.find_element(By.ID, "result")
        self.assertTrue("press:" in result.text)

    def testShouldFireKeyDownEvents(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        keyReporter = self.marionette.find_element(By.ID, "keyReporter")
        keyReporter.send_keys("I")
        result = self.marionette.find_element(By.ID, "result")
        self.assertTrue("down" in result.text)

    def testShouldFireKeyUpEvents(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element(By.ID, "keyReporter")
        keyReporter.send_keys("a")
        result = self.marionette.find_element(By.ID, "result")
        self.assertTrue("up:" in result.text)

    def testShouldTypeLowerCaseLetters(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element(By.ID, "keyReporter")
        keyReporter.send_keys("abc def")
        self.assertEqual("abc def", keyReporter.get_property("value"))

    def testShouldBeAbleToTypeCapitalLetters(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element(By.ID, "keyReporter")
        keyReporter.send_keys("ABC DEF")
        self.assertEqual("ABC DEF", keyReporter.get_property("value"))

    def testCutAndPasteShortcuts(self):
        # test that modifier keys work via copy/paste shortcuts
        if self.marionette.session_capabilities["platformName"] == "darwin":
            mod_key = Keys.META
        else:
            mod_key = Keys.CONTROL

        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element(By.ID, "keyReporter")
        self.assertEqual("", keyReporter.get_property("value"))
        keyReporter.send_keys("zyxwvutsr")
        self.assertEqual("zyxwvutsr", keyReporter.get_property("value"))

        # select all and cut
        keyReporter.send_keys(mod_key, "a")
        keyReporter.send_keys(mod_key, "x")
        self.assertEqual("", keyReporter.get_property("value"))

        with self.marionette.using_context("chrome"):
            url_bar = self.marionette.find_element(By.ID, "urlbar")

            # lear and paste
            url_bar.send_keys(mod_key, "a")
            url_bar.send_keys(Keys.BACK_SPACE)

            self.assertEqual("", url_bar.get_property("value"))
            url_bar.send_keys(mod_key, "v")
            self.assertEqual("zyxwvutsr", url_bar.get_property("value"))

    def testShouldBeAbleToTypeQuoteMarks(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element(By.ID, "keyReporter")
        keyReporter.send_keys("\"")
        self.assertEqual("\"", keyReporter.get_property("value"))

    def testShouldBeAbleToTypeTheAtCharacter(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element(By.ID, "keyReporter")
        keyReporter.send_keys("@")
        self.assertEqual("@", keyReporter.get_property("value"))

    def testShouldBeAbleToMixUpperAndLowerCaseLetters(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element(By.ID, "keyReporter")
        keyReporter.send_keys("me@eXample.com")
        self.assertEqual("me@eXample.com", keyReporter.get_property("value"))

    def testArrowKeysShouldNotBePrintable(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        keyReporter = self.marionette.find_element(By.ID, "keyReporter")
        keyReporter.send_keys(Keys.ARROW_LEFT)
        self.assertEqual("", keyReporter.get_property("value"))

    def testWillSimulateAKeyUpWhenEnteringTextIntoInputElements(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyUp")
        element.send_keys("I like cheese")
        result = self.marionette.find_element(By.ID, "result")
        self.assertEqual(result.text, "I like cheese")

    def testWillSimulateAKeyDownWhenEnteringTextIntoInputElements(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyDown")
        element.send_keys("I like cheese")
        result = self.marionette.find_element(By.ID, "result")
        #  Because the key down gets the result before the input element is
        #  filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def testWillSimulateAKeyPressWhenEnteringTextIntoInputElements(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyPress")
        element.send_keys("I like cheese")
        result = self.marionette.find_element(By.ID, "result")
        #  Because the key down gets the result before the input element is
        #  filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def testWillSimulateAKeyUpWhenEnteringTextIntoTextAreas(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyUpArea")
        element.send_keys("I like cheese")
        result = self.marionette.find_element(By.ID, "result")
        self.assertEqual("I like cheese", result.text)

    def testWillSimulateAKeyDownWhenEnteringTextIntoTextAreas(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyDownArea")
        element.send_keys("I like cheese")
        result = self.marionette.find_element(By.ID, "result")
        #  Because the key down gets the result before the input element is
        #  filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def testWillSimulateAKeyPressWhenEnteringTextIntoTextAreas(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyPressArea")
        element.send_keys("I like cheese")
        result = self.marionette.find_element(By.ID, "result")
        #  Because the key down gets the result before the input element is
        #  filled, we're a letter short here
        self.assertEqual(result.text, "I like chees")

    def testShouldReportKeyCodeOfArrowKeysUpDownEvents(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        result = self.marionette.find_element(By.ID, "result")
        element = self.marionette.find_element(By.ID, "keyReporter")
        element.send_keys(Keys.ARROW_DOWN)
        self.assertTrue("down: 40" in result.text.strip())
        self.assertTrue("up: 40" in result.text.strip())

        element.send_keys(Keys.ARROW_UP)
        self.assertTrue("down: 38" in  result.text.strip())
        self.assertTrue("up: 38" in result.text.strip())

        element.send_keys(Keys.ARROW_LEFT)
        self.assertTrue("down: 37" in result.text.strip())
        self.assertTrue("up: 37" in result.text.strip())

        element.send_keys(Keys.ARROW_RIGHT)
        self.assertTrue("down: 39" in result.text.strip())
        self.assertTrue("up: 39" in result.text.strip())

        #  And leave no rubbish/printable keys in the "keyReporter"
        self.assertEqual("", element.get_property("value"))

    """Disabled. Reenable in Bug 1068728
    def testNumericShiftKeys(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        result = self.marionette.find_element(By.ID, "result")
        element = self.marionette.find_element(By.ID, "keyReporter")
        numericShiftsEtc = "~!@#$%^&*()_+{}:i\"<>?|END~"
        element.send_keys(numericShiftsEtc)
        self.assertEqual(numericShiftsEtc, element.get_property("value"))
        self.assertTrue(" up: 16" in result.text.strip())
    """

    def testLowerCaseAlphaKeys(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyReporter")
        lowerAlphas = "abcdefghijklmnopqrstuvwxyz"
        element.send_keys(lowerAlphas)
        self.assertEqual(lowerAlphas, element.get_property("value"))

    """Disabled. Reenable in Bug 1068735
    def testUppercaseAlphaKeys(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        result = self.marionette.find_element(By.ID, "result")
        element = self.marionette.find_element(By.ID, "keyReporter")
        upperAlphas = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        element.send_keys(upperAlphas)
        self.assertEqual(upperAlphas, element.get_property("value"))
        self.assertTrue(" up: 16" in result.text.strip())
    """

    """Disabled. Reenable in Bug 1068726
    def testAllPrintableKeys(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        result = self.marionette.find_element(By.ID, "result")
        element = self.marionette.find_element(By.ID, "keyReporter")
        allPrintable = "!\"#$%&'()*+,-./0123456789:<=>?@ ABCDEFGHIJKLMNOPQRSTUVWXYZ [\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
        element.send_keys(allPrintable)

        self.assertTrue(allPrintable, element.get_property("value"))
        self.assertTrue(" up: 16" in result.text.strip())
    """

    """Disabled. Reenable in Bug 1068733
    def testSpecialSpaceKeys(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyReporter")
        element.send_keys("abcd" + Keys.SPACE + "fgh" + Keys.SPACE + "ij")
        self.assertEqual("abcd fgh ij", element.get_property("value"))
    """

    def testShouldTypeAnInteger(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "keyReporter")
        element.send_keys(1234)
        self.assertEqual("1234", element.get_property("value"))

    def testShouldSendKeysToElementsWithoutTheValueAttribute(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        # If we don't get an error below we are good
        self.marionette.find_element(By.TAG_NAME, "body").send_keys("foo")

    def testShouldThrowElementNotVisibleWhenInputHidden(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        not_displayed = self.marionette.find_element(By.ID, "notDisplayed")
        self.assertRaises(ElementNotVisibleException, not_displayed.send_keys, "foo")

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

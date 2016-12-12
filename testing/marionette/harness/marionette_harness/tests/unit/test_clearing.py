# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.errors import InvalidElementStateException

from marionette_harness import MarionetteTestCase


class TestClear(MarionetteTestCase):
    def testWriteableTextInputShouldClear(self):
        test_html = self.marionette.absolute_url("test_clearing.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID, "writableTextInput")
        element.clear()
        self.assertEqual("", element.get_property("value"))

    def testTextInputShouldNotClearWhenReadOnly(self):
        test_html = self.marionette.absolute_url("test_clearing.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID,"readOnlyTextInput")
        try:
            element.clear()
            self.fail("Should not have been able to clear")
        except InvalidElementStateException:
            pass

    def testWritableTextAreaShouldClear(self):
        test_html = self.marionette.absolute_url("test_clearing.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID,"writableTextArea")
        element.clear()
        self.assertEqual("", element.get_property("value"))

    def testTextAreaShouldNotClearWhenDisabled(self):
        test_html = self.marionette.absolute_url("test_clearing.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID,"textAreaNotenabled")
        try:
            element.clear()
            self.fail("Should not have been able to clear")
        except InvalidElementStateException:
            pass

    def testTextAreaShouldNotClearWhenReadOnly(self):
        test_html = self.marionette.absolute_url("test_clearing.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID,"textAreaReadOnly")
        try:
            element.clear()
            self.fail("Should not have been able to clear")
        except InvalidElementStateException:
            pass

    def testContentEditableAreaShouldClear(self):
        test_html = self.marionette.absolute_url("test_clearing.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID,"content-editable")
        element.clear()
        self.assertEqual("", element.text)

    def testTextInputShouldNotClearWhenDisabled(self):
        test_html = self.marionette.absolute_url("test_clearing.html")
        self.marionette.navigate(test_html)
        try:
            element = self.marionette.find_element(By.ID,"textInputnotenabled")
            self.assertFalse(element.is_enabled())
            element.clear()
            self.fail("Should not have been able to clear")
        except InvalidElementStateException:
            pass

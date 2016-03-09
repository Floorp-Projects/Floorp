# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.errors import (ElementNotAccessibleException,
                                      ElementNotVisibleException)


class TestAccessibility(MarionetteTestCase):

    # Elements that are accessible with and without the accessibliity API
    valid_elementIDs = [
        # Button1 is an accessible button with a valid accessible name
        # computed from subtree
        "button1",
        # Button2 is an accessible button with a valid accessible name
        # computed from aria-label
        "button2",
        # Button13 is an accessible button that is implemented via role="button"
        # and is explorable using tabindex="0"
        "button13",
        # button17 is an accessible button that overrides parent's
        # pointer-events:none; property with its own pointer-events:all;
        "button17"
    ]

    # Elements that are not accessible with the accessibility API
    invalid_elementIDs = [
        # Button3 does not have an accessible object
        "button3",
        # Button4 does not support any accessible actions
        "button4",
        # Button5 does not have a correct accessibility role and may not be
        # manipulated via the accessibility API
        "button5",
        # Button6 is missing an accesible name
        "button6",
        # Button7 is not currently visible via the accessibility API and may
        # not be manipulated by it
        "button7",
        # Button8 is not currently visible via the accessibility API and may
        # not be manipulated by it (in hidden subtree)
        "button8",
        # Button14 is accessible button but is not explorable because of lack
        # of tabindex that would make it focusable.
        "button14"
    ]

    # Elements that are either accessible to accessibility API or not accessible
    # at all
    falsy_elements = [
        # Element is only visible to the accessibility API and may be
        # manipulated by it
        "button9",
        # Element is not currently visible
        "button10"
    ]

    displayed_elementIDs = [
        "button1", "button2", "button3", "button4", "button5", "button6",
        "button9", "no_accessible_but_displayed"
    ]

    displayed_but_a11y_hidden_elementIDs = ["button7", "button8"]

    disabled_elementIDs = ["button11", "no_accessible_but_disabled"]

    # Elements that are enabled but otherwise disabled or not explorable via the accessibility API
    disabled_accessibility_elementIDs = ["button12", "button15", "button16"]

    # Elements that are reporting selected state
    valid_option_elementIDs = ["option1", "option2"]

    def run_element_test(self, ids, testFn):
        for id in ids:
            element = self.marionette.find_element("id", id)
            testFn(element)

    def setup_accessibility(self, raisesAccessibilityExceptions=True, navigate=True):
        self.marionette.delete_session()
        self.marionette.start_session(
            {"raisesAccessibilityExceptions": raisesAccessibilityExceptions})
        # Navigate to test_accessibility.html
        if navigate:
            test_accessibility = self.marionette.absolute_url("test_accessibility.html")
            self.marionette.navigate(test_accessibility)

    def test_valid_single_tap(self):
        self.setup_accessibility()
        # No exception should be raised
        self.run_element_test(self.valid_elementIDs, lambda button: button.tap())

    def test_single_tap_raises_element_not_accessible(self):
        self.setup_accessibility()
        self.run_element_test(self.invalid_elementIDs,
                              lambda button: self.assertRaises(ElementNotAccessibleException,
                                                               button.tap))
        self.run_element_test(self.falsy_elements,
                              lambda button: self.assertRaises(ElementNotVisibleException,
                                                               button.tap))

    def test_single_tap_raises_no_exceptions(self):
        self.setup_accessibility(False, True)
        # No exception should be raised
        self.run_element_test(self.invalid_elementIDs, lambda button: button.tap())
        # Elements are invisible
        self.run_element_test(self.falsy_elements,
                              lambda button: self.assertRaises(ElementNotVisibleException,
                                                               button.tap))

    def test_valid_click(self):
        self.setup_accessibility()
        # No exception should be raised
        self.run_element_test(self.valid_elementIDs, lambda button: button.click())

    def test_click_raises_element_not_accessible(self):
        self.setup_accessibility()
        self.run_element_test(self.invalid_elementIDs,
                              lambda button: self.assertRaises(ElementNotAccessibleException,
                                                               button.click))
        self.run_element_test(self.falsy_elements,
                              lambda button: self.assertRaises(ElementNotVisibleException,
                                                               button.click))

    def test_click_raises_no_exceptions(self):
        self.setup_accessibility(False, True)
        # No exception should be raised
        self.run_element_test(self.invalid_elementIDs, lambda button: button.click())
        # Elements are invisible
        self.run_element_test(self.falsy_elements,
                              lambda button: self.assertRaises(ElementNotVisibleException,
                                                               button.click))

    def test_element_visible_but_not_visible_to_accessbility(self):
        self.setup_accessibility()
        # Elements are displayed but hidden from accessibility API
        self.run_element_test(self.displayed_but_a11y_hidden_elementIDs,
                              lambda element: self.assertRaises(ElementNotAccessibleException,
                                                                element.is_displayed))

    def test_element_is_visible_to_accessibility(self):
        self.setup_accessibility()
        # No exception should be raised
        self.run_element_test(self.displayed_elementIDs, lambda element: element.is_displayed())

    def test_element_is_not_enabled_to_accessbility(self):
        self.setup_accessibility()
        # Buttons are enabled but disabled/not-explorable via the accessibility API
        self.run_element_test(self.disabled_accessibility_elementIDs,
                              lambda element: self.assertRaises(ElementNotAccessibleException,
                                                                element.is_enabled))

        # Buttons are enabled but disabled/not-explorable via the accessibility API and thus are not
        # clickable via the accessibility API
        self.run_element_test(self.disabled_accessibility_elementIDs,
                              lambda element: self.assertRaises(ElementNotAccessibleException,
                                                                element.click))

        self.setup_accessibility(False, False)
        self.run_element_test(self.disabled_accessibility_elementIDs,
                              lambda element: element.is_enabled())
        self.run_element_test(self.disabled_accessibility_elementIDs,
                              lambda element: element.click())

    def test_element_is_enabled_to_accessibility(self):
        self.setup_accessibility()
        # No exception should be raised
        self.run_element_test(self.disabled_elementIDs, lambda element: element.is_enabled())

    def test_send_keys_raises_no_exception(self):
        self.setup_accessibility()
        # Sending keys to valid input should not raise any exceptions
        self.run_element_test(['input1'], lambda element: element.send_keys("a"))

        self.setup_accessibility(False, False)
        # Sending keys to invalid element should not raise any exceptions when raising accessibility
        # exceptions is disabled
        self.run_element_test(['button5'], lambda element: element.send_keys("abc"))

    def test_send_keys_raises_element_not_accessible(self):
        self.setup_accessibility()
        # Sending keys to invalid element should raise an exception
        self.run_element_test(['button5'],
                              lambda element: self.assertRaises(ElementNotAccessibleException,
                                                                element.send_keys))

    def test_is_selected_raises_no_exception(self):
        self.setup_accessibility()
        # No exception should be raised for valid options
        self.run_element_test(self.valid_option_elementIDs, lambda element: element.is_selected())
        # No exception should be raised for non-selectable elements
        self.run_element_test(self.valid_elementIDs, lambda element: element.is_selected())

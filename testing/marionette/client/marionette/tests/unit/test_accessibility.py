# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from errors import ElementNotAccessibleException
from errors import ElementNotVisibleException


class TestAccessibility(MarionetteTestCase):

    # Elements that are accessible with and without the accessibliity API
    valid_elementIDs = [
        # Button1 is an accessible button with a valid accessible name
        # computed from subtree
        "button1",
        # Button2 is an accessible button with a valid accessible name
        # computed from aria-label
        "button2"
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
        "button8"
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

    def test_invalid_single_tap(self):
        self.setup_accessibility()
        self.run_element_test(self.invalid_elementIDs,
                              lambda button: self.assertRaises(ElementNotAccessibleException,
                                                               button.tap))
        self.run_element_test(self.falsy_elements,
                              lambda button: self.assertRaises(ElementNotAccessibleException,
                                                               button.tap))

    def test_invalid_single_tap_no_exceptions(self):
        self.setup_accessibility(False, True)
        # No exception should be raised
        self.run_element_test(self.invalid_elementIDs, lambda button: button.tap())
        # Elements are invisible
        self.run_element_test(self.falsy_elements,
                              lambda button: self.assertRaises(ElementNotVisibleException,
                                                               button.tap))

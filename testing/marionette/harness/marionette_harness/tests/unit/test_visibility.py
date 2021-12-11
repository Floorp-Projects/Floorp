# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from six.moves.urllib.parse import quote

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


def element_direction_doc(direction):
    return inline(
        """
        <meta name="viewport" content="initial-scale=1,width=device-width">
        <style>
          .element{{
            position: absolute;
            {}: -50px;
            background_color: red;
            width: 100px;
            height: 100px;
          }}
        </style>
        <div class='element'></div>""".format(
            direction
        )
    )


class TestVisibility(MarionetteTestCase):
    def testShouldAllowTheUserToTellIfAnElementIsDisplayedOrNot(self):
        test_html = self.marionette.absolute_url("visibility.html")
        self.marionette.navigate(test_html)

        self.assertTrue(self.marionette.find_element(By.ID, "displayed").is_displayed())
        self.assertFalse(self.marionette.find_element(By.ID, "none").is_displayed())
        self.assertFalse(
            self.marionette.find_element(By.ID, "suppressedParagraph").is_displayed()
        )
        self.assertFalse(self.marionette.find_element(By.ID, "hidden").is_displayed())

    def testVisibilityShouldTakeIntoAccountParentVisibility(self):
        test_html = self.marionette.absolute_url("visibility.html")
        self.marionette.navigate(test_html)

        childDiv = self.marionette.find_element(By.ID, "hiddenchild")
        hiddenLink = self.marionette.find_element(By.ID, "hiddenlink")

        self.assertFalse(childDiv.is_displayed())
        self.assertFalse(hiddenLink.is_displayed())

    def testShouldCountElementsAsVisibleIfStylePropertyHasBeenSet(self):
        test_html = self.marionette.absolute_url("visibility.html")
        self.marionette.navigate(test_html)
        shown = self.marionette.find_element(By.ID, "visibleSubElement")
        self.assertTrue(shown.is_displayed())

    def testShouldModifyTheVisibilityOfAnElementDynamically(self):
        test_html = self.marionette.absolute_url("visibility.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element(By.ID, "hideMe")
        self.assertTrue(element.is_displayed())
        element.click()
        self.assertFalse(element.is_displayed())

    def testHiddenInputElementsAreNeverVisible(self):
        test_html = self.marionette.absolute_url("visibility.html")
        self.marionette.navigate(test_html)

        shown = self.marionette.find_element(By.NAME, "hidden")

        self.assertFalse(shown.is_displayed())

    def test_elements_not_displayed_with_negative_transform(self):
        self.marionette.navigate(
            inline(
                """
            <div id="y" style="transform: translateY(-200%);">hidden</div>
            <div id="x" style="transform: translateX(-200%);">hidden</div>
        """
            )
        )

        element_x = self.marionette.find_element(By.ID, "x")
        self.assertFalse(element_x.is_displayed())
        element_y = self.marionette.find_element(By.ID, "y")
        self.assertFalse(element_y.is_displayed())

    def test_elements_not_displayed_with_parents_having_negative_transform(self):
        self.marionette.navigate(
            inline(
                """
            <div style="transform: translateY(-200%);"><p id="y">hidden</p></div>
            <div style="transform: translateX(-200%);"><p id="x">hidden</p></div>
        """
            )
        )

        element_x = self.marionette.find_element(By.ID, "x")
        self.assertFalse(element_x.is_displayed())
        element_y = self.marionette.find_element(By.ID, "y")
        self.assertFalse(element_y.is_displayed())

    def test_element_displayed_with_zero_transform(self):
        self.marionette.navigate(
            inline(
                """
            <div style="transform: translate(0px, 0px);">not hidden</div>
        """
            )
        )
        element = self.marionette.find_element(By.TAG_NAME, "div")
        self.assertTrue(element.is_displayed())

    def test_element_displayed_with_negative_transform_but_in_viewport(self):
        self.marionette.navigate(
            inline(
                """
            <div style="margin-top: 1em; transform: translateY(-75%);">not hidden</div>
            """
            )
        )
        element = self.marionette.find_element(By.TAG_NAME, "div")
        self.assertTrue(element.is_displayed())

    def testShouldSayElementIsInvisibleWhenOverflowXIsHiddenAndOutOfViewport(self):
        test_html = self.marionette.absolute_url("bug814037.html")
        self.marionette.navigate(test_html)
        overflow_x = self.marionette.find_element(By.ID, "assertMe2")
        self.assertFalse(overflow_x.is_displayed())

    def testShouldShowElementNotVisibleWithHiddenAttribute(self):
        self.marionette.navigate(
            inline(
                """
            <p hidden>foo</p>
        """
            )
        )
        singleHidden = self.marionette.find_element(By.TAG_NAME, "p")
        self.assertFalse(singleHidden.is_displayed())

    def testShouldShowElementNotVisibleWhenParentElementHasHiddenAttribute(self):
        self.marionette.navigate(
            inline(
                """
            <div hidden>
                <p>foo</p>
            </div>
        """
            )
        )
        child = self.marionette.find_element(By.TAG_NAME, "p")
        self.assertFalse(child.is_displayed())

    def testShouldClickOnELementPartiallyOffLeft(self):
        test_html = self.marionette.navigate(element_direction_doc("left"))
        self.marionette.find_element(By.CSS_SELECTOR, ".element").click()

    def testShouldClickOnELementPartiallyOffRight(self):
        test_html = self.marionette.navigate(element_direction_doc("right"))
        self.marionette.find_element(By.CSS_SELECTOR, ".element").click()

    def testShouldClickOnELementPartiallyOffTop(self):
        test_html = self.marionette.navigate(element_direction_doc("top"))
        self.marionette.find_element(By.CSS_SELECTOR, ".element").click()

    def testShouldClickOnELementPartiallyOffBottom(self):
        test_html = self.marionette.navigate(element_direction_doc("bottom"))
        self.marionette.find_element(By.CSS_SELECTOR, ".element").click()

from marionette_test import MarionetteTestCase

class TestVisibility(MarionetteTestCase):

    def testShouldAllowTheUserToTellIfAnElementIsDisplayedOrNot(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        self.assertTrue(self.marionette.find_element('id', "displayed").is_displayed())
        self.assertFalse(self.marionette.find_element('id', "none").is_displayed())
        self.assertFalse(self.marionette.find_element('id',
            "suppressedParagraph").is_displayed())
        self.assertFalse(self.marionette.find_element('id', "hidden").is_displayed())

    def testVisibilityShouldTakeIntoAccountParentVisibility(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        childDiv = self.marionette.find_element('id', "hiddenchild")
        hiddenLink = self.marionette.find_element('id', "hiddenlink")

        self.assertFalse(childDiv.is_displayed())
        self.assertFalse(hiddenLink.is_displayed())

    def testShouldCountElementsAsVisibleIfStylePropertyHasBeenSet(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        shown = self.marionette.find_element('id', "visibleSubElement")
        self.assertTrue(shown.is_displayed())

    def testShouldModifyTheVisibilityOfAnElementDynamically(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)
        element = self.marionette.find_element('id', "hideMe")
        self.assertTrue(element.is_displayed())
        element.click()
        self.assertFalse(element.is_displayed())

    def testHiddenInputElementsAreNeverVisible(self):
        test_html = self.marionette.absolute_url("javascriptPage.html")
        self.marionette.navigate(test_html)

        shown = self.marionette.find_element('name', "hidden")

        self.assertFalse(shown.is_displayed())

    def testShouldSayElementsWithNegativeTransformAreNotDisplayed(self):
        test_html = self.marionette.absolute_url("cssTransform.html")
        self.marionette.navigate(test_html)

        elementX = self.marionette.find_element("id", 'parentX')
        self.assertFalse(elementX.is_displayed())
        elementY = self.marionette.find_element("id", 'parentY')
        self.assertFalse(elementY.is_displayed())

    def testShouldSayElementsWithParentWithNegativeTransformAreNotDisplayed(self):
        test_html = self.marionette.absolute_url("cssTransform.html")
        self.marionette.navigate(test_html)

        elementX = self.marionette.find_element("id", 'childX')
        self.assertFalse(elementX.is_displayed())
        elementY = self.marionette.find_element("id", 'childY')
        self.assertFalse(elementY.is_displayed())

    def testShouldSayElementWithZeroTransformIsVisible(self):
        test_html = self.marionette.absolute_url("cssTransform.html")
        self.marionette.navigate(test_html)

        zero_tranform = self.marionette.find_element("id", 'zero-tranform')
        self.assertTrue(zero_tranform.is_displayed())

    def testShouldSayElementIsVisibleWhenItHasNegativeTransformButElementisntInANegativeSpace(self):
        test_html = self.marionette.absolute_url("cssTransform2.html")
        self.marionette.navigate(test_html)
        negative_percent__tranform = self.marionette.find_element("id", 'negative-percentage-transformY')
        self.assertTrue(negative_percent__tranform.is_displayed())

    def testShouldSayElementIsInvisibleWhenOverflowXIsHiddenAndOutOfViewport(self):
        test_html = self.marionette.absolute_url("bug814037.html")
        self.marionette.navigate(test_html)
        overflow_x = self.marionette.find_element("id", "assertMe2")
        self.assertFalse(overflow_x.is_displayed())

    def testShouldShowElementNotVisibleWithHiddenAttribute(self):
        test_html = self.marionette.absolute_url("hidden.html")
        self.marionette.navigate(test_html)
        singleHidden = self.marionette.find_element('id', 'singleHidden')
        self.assertFalse(singleHidden.is_displayed())

    def testShouldShowElementNotVisibleWhenParentElementHasHiddenAttribute(self):
        test_html = self.marionette.absolute_url("hidden.html")
        self.marionette.navigate(test_html)
        child = self.marionette.find_element('id', 'child')
        self.assertFalse(child.is_displayed())

    def testShouldClickOnELementPartiallyOffLeft(self):
        test_html = self.marionette.absolute_url("element_left.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element('css selector', '.element').click()

    def testShouldClickOnELementPartiallyOffRight(self):
        test_html = self.marionette.absolute_url("element_right.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element('css selector', '.element').click()

    def testShouldClickOnELementPartiallyOffTop(self):
        test_html = self.marionette.absolute_url("element_top.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element('css selector', '.element').click()

    def testShouldClickOnELementPartiallyOffBottom(self):
        test_html = self.marionette.absolute_url("element_bottom.html")
        self.marionette.navigate(test_html)
        self.marionette.find_element('css selector', '.element').click()

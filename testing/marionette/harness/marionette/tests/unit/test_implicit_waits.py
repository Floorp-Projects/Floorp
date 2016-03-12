# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver.errors import NoSuchElementException
from marionette_driver.by import By


class TestImplicitWaits(MarionetteTestCase):
    def testShouldImplicitlyWaitForASingleElement(self):
        test_html = self.marionette.absolute_url("test_dynamic.html")
        self.marionette.navigate(test_html)
        add = self.marionette.find_element(By.ID, "adder")
        self.marionette.set_search_timeout("30000")
        add.click()
        # All is well if this doesnt throw
        self.marionette.find_element(By.ID, "box0")

    def testShouldStillFailToFindAnElementWhenImplicitWaitsAreEnabled(self):
        test_html = self.marionette.absolute_url("test_dynamic.html")
        self.marionette.navigate(test_html)
        self.marionette.set_search_timeout("3000")
        try:
            self.marionette.find_element(By.ID, "box0")
            self.fail("Should have thrown a a NoSuchElementException")
        except NoSuchElementException:
            pass
        except Exception:
            self.fail("Should have thrown a NoSuchElementException")

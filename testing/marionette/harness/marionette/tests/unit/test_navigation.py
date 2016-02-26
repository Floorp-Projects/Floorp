# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette.marionette_test import MarionetteTestCase, skip_if_b2g
from marionette_driver.errors import MarionetteException, TimeoutException

class TestNavigate(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.execute_script("window.location.href = 'about:blank'")
        self.assertEqual("about:blank", self.location_href)
        self.test_doc = self.marionette.absolute_url("test.html")
        self.iframe_doc = self.marionette.absolute_url("test_iframe.html")

    def test_set_location_through_execute_script(self):
        self.marionette.execute_script("window.location.href = '%s'" % self.test_doc)
        self.assertEqual(self.test_doc, self.location_href)
        self.assertEqual("Marionette Test", self.marionette.title)

    def test_navigate(self):
        self.marionette.navigate(self.test_doc)
        self.assertNotEqual("about:blank", self.location_href)
        self.assertEqual("Marionette Test", self.marionette.title)

    @skip_if_b2g # we currently allow this in b2g
    def test_navigate_chrome_error(self):
        with self.marionette.using_context("chrome"):
            self.assertRaisesRegexp(MarionetteException, "Cannot navigate in chrome context",
                                    self.marionette.navigate, "about:blank")

    def test_get_current_url_returns_top_level_browsing_context_url(self):
        self.marionette.navigate(self.iframe_doc)
        self.assertEqual(self.iframe_doc, self.location_href)
        frame = self.marionette.find_element("css selector", "#test_iframe")
        self.marionette.switch_to_frame(frame)
        self.assertEqual(self.iframe_doc, self.marionette.get_url())

    def test_get_current_url(self):
        self.marionette.navigate(self.test_doc)
        self.assertEqual(self.test_doc, self.marionette.get_url())
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.marionette.get_url())

    def test_go_back(self):
        self.marionette.navigate(self.test_doc)
        self.assertNotEqual("about:blank", self.location_href)
        self.assertEqual("Marionette Test", self.marionette.title)
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.location_href)
        self.marionette.go_back()
        self.assertNotEqual("about:blank", self.location_href)
        self.assertEqual("Marionette Test", self.marionette.title)

    def test_go_forward(self):
        self.marionette.navigate(self.test_doc)
        self.assertNotEqual("about:blank", self.location_href)
        self.assertEqual("Marionette Test", self.marionette.title)
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.location_href)
        self.marionette.go_back()
        self.assertNotEqual("about:blank", self.location_href)
        self.assertEqual("Marionette Test", self.marionette.title)
        self.marionette.go_forward()
        self.assertEqual("about:blank", self.location_href)

    def test_refresh(self):
        self.marionette.navigate(self.test_doc)
        self.assertEqual("Marionette Test", self.marionette.title)
        self.assertTrue(self.marionette.execute_script(
            """var elem = window.document.createElement('div'); elem.id = 'someDiv';
            window.document.body.appendChild(elem); return true;"""))
        self.assertFalse(self.marionette.execute_script(
            "return window.document.getElementById('someDiv') == undefined"))
        self.marionette.refresh()
        self.assertEqual("Marionette Test", self.marionette.title)
        self.assertTrue(self.marionette.execute_script(
            "return window.document.getElementById('someDiv') == undefined"))

    """ Disabled due to Bug 977899
    @skip_if_b2g
    def test_navigate_frame(self):
        self.marionette.navigate(self.marionette.absolute_url("test_iframe.html"))
        self.marionette.switch_to_frame(0)
        self.marionette.navigate(self.marionette.absolute_url("empty.html"))
        self.assertTrue('empty.html' in self.marionette.get_url())
        self.marionette.switch_to_frame()
        self.assertTrue('test_iframe.html' in self.marionette.get_url())
    """

    def test_should_not_error_if_nonexistent_url_used(self):
        try:
            self.marionette.navigate("thisprotocoldoesnotexist://")
            self.fail("Should have thrown a MarionetteException")
        except TimeoutException:
            self.fail("The socket shouldn't have timed out when navigating to a non-existent URL")
        except MarionetteException as e:
            self.assertIn("Error loading page", str(e))
        except Exception as e:
            import traceback
            print traceback.format_exc()
            self.fail("Should have thrown a MarionetteException instead of %s" % type(e))

    @skip_if_b2g # about:blocked isn't a well formed uri on b2g
    def test_should_navigate_to_requested_about_page(self):
        self.marionette.navigate("about:neterror")
        self.assertEqual(self.marionette.get_url(), "about:neterror")
        self.marionette.navigate(self.marionette.absolute_url("test.html"))
        self.marionette.navigate("about:blocked")
        self.assertEqual(self.marionette.get_url(), "about:blocked")

    def test_find_element_state_complete(self):
        self.marionette.navigate(self.test_doc)
        state = self.marionette.execute_script("return window.document.readyState")
        self.assertEqual("complete", state)
        self.assertTrue(self.marionette.find_element("id", "mozLink"))

    def test_should_throw_a_timeoutexception_when_loading_page(self):
        try:
            self.marionette.timeouts("page load", 0)
            self.marionette.navigate(self.test_doc)
            self.assertTrue(self.marionette.find_element("id", "mozLink"))
            self.fail("Should have thrown a MarionetteException")
        except TimeoutException as e:
            self.assertTrue("Error loading page, timed out" in str(e))
        except Exception as e:
            import traceback
            print traceback.format_exc()
            self.fail("Should have thrown a TimeoutException instead of %s" % type(e))

    def test_navigate_iframe(self):
        self.marionette.navigate(self.iframe_doc)
        self.assertTrue('test_iframe.html' in self.marionette.get_url())
        self.assertTrue(self.marionette.find_element("id", "test_iframe"))

    @property
    def location_href(self):
        return self.marionette.execute_script("return window.location.href")

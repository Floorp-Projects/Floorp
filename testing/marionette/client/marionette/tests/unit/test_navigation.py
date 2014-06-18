# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from marionette import MarionetteException
from marionette import TimeoutException

class TestNavigate(MarionetteTestCase):
    def test_navigate(self):
        self.assertTrue(self.marionette.execute_script("window.location.href = 'about:blank'; return true;"))
        self.assertEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertNotEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.assertEqual("Marionette Test", self.marionette.title)

    def test_getUrl(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertTrue(test_html in self.marionette.get_url())
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.marionette.get_url())

    def test_goBack(self):
        self.assertTrue(self.marionette.execute_script("window.location.href = 'about:blank'; return true;"))
        self.assertEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertNotEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.assertEqual("Marionette Test", self.marionette.title)
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.marionette.go_back()
        self.assertNotEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.assertEqual("Marionette Test", self.marionette.title)

    def test_goForward(self):
        self.assertTrue(self.marionette.execute_script("window.location.href = 'about:blank'; return true;"))
        self.assertEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertNotEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.assertEqual("Marionette Test", self.marionette.title)
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.marionette.go_back()
        self.assertNotEqual("about:blank", self.marionette.execute_script("return window.location.href;"))
        self.assertEqual("Marionette Test", self.marionette.title)
        self.marionette.go_forward()
        self.assertEqual("about:blank", self.marionette.execute_script("return window.location.href;"))

    def test_refresh(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertEqual("Marionette Test", self.marionette.title)
        self.assertTrue(self.marionette.execute_script("var elem = window.document.createElement('div'); elem.id = 'someDiv';" +
                                        "window.document.body.appendChild(elem); return true;"))
        self.assertFalse(self.marionette.execute_script("return window.document.getElementById('someDiv') == undefined;"))
        self.marionette.refresh()
        self.assertEqual("Marionette Test", self.marionette.title)
        self.assertTrue(self.marionette.execute_script("return window.document.getElementById('someDiv') == undefined;"))

    def test_navigate_frame(self):
        self.marionette.navigate(self.marionette.absolute_url("test_iframe.html"))
        self.marionette.switch_to_frame(0)
        self.marionette.navigate(self.marionette.absolute_url("empty.html"))
        self.assertTrue('empty.html' in self.marionette.get_url())
        self.marionette.switch_to_frame()
        self.assertTrue('test_iframe.html' in self.marionette.get_url())

    def test_shouldnt_error_if_nonexistent_url_used(self):
        try:
            self.marionette.navigate("thisprotocoldoesnotexist://")
            self.fail("Should have thrown a MarionetteException")
        except TimeoutException:
            self.fail("The socket shouldn't have timed out when navigating to a non-existent URL")
        except MarionetteException as e:
            self.assertIn("Error loading page", str(e))
        except Exception as inst:
            import traceback
            print traceback.format_exc()
            self.fail("Should have thrown a MarionetteException instead of %s" % type(inst))

    def test_find_element_state_complete(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        state = self.marionette.execute_script("return window.document.readyState;")
        self.assertEqual("complete", state)
        self.assertTrue(self.marionette.find_element("id", "mozLink"))

    def test_should_throw_a_timeoutexception_when_loading_page(self):
        try:
            self.marionette.timeouts("page load", 0)
            test_html = self.marionette.absolute_url("test.html")
            self.marionette.navigate(test_html)
            self.assertTrue(self.marionette.find_element("id", "mozLink"))
            self.fail("Should have thrown a MarionetteException")
        except MarionetteException as e:
            self.assertTrue("Error loading page, timed out" in str(e))
        except Exception as inst:
            import traceback
            print traceback.format_exc()
            self.fail("Should have thrown a MarionetteException instead of %s" % type(inst))

    def test_navigate_iframe(self):
        test_iframe = self.marionette.absolute_url("test_iframe.html")
        self.marionette.navigate(test_iframe)
        self.assertTrue('test_iframe.html' in self.marionette.get_url())
        self.assertTrue(self.marionette.find_element("id", "test_iframe"))

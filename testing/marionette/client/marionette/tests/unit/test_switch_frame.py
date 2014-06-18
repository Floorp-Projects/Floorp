# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from marionette import JavascriptException


class TestSwitchFrame(MarionetteTestCase):
    def test_switch_simple(self):
        start_url = "test_iframe.html"
        verify_title = "Marionette IFrame Test"
        verify_url = "test.html"
        test_html = self.marionette.absolute_url(start_url)
        self.marionette.navigate(test_html)
        self.assertEqual(self.marionette.get_active_frame(), None)
        frame = self.marionette.find_element("id", "test_iframe")
        self.marionette.switch_to_frame(frame)
        self.assertTrue(verify_url in self.marionette.get_url())
        inner_frame_element = self.marionette.get_active_frame()
        # test that we can switch back to main frame, then switch back to the
        # inner frame with the value we got from get_active_frame
        self.marionette.switch_to_frame()
        self.assertEqual(verify_title, self.marionette.title)
        self.marionette.switch_to_frame(inner_frame_element)
        self.assertTrue(verify_url in self.marionette.get_url())

    def test_switch_nested(self):
        start_url = "test_nested_iframe.html"
        verify_title = "Marionette IFrame Test"
        verify_url = "test_inner_iframe.html"
        test_html = self.marionette.absolute_url(start_url)
        self.marionette.navigate(test_html)
        frame = self.marionette.find_element("id", "test_iframe")
        self.assertEqual(self.marionette.get_active_frame(), None)
        self.marionette.switch_to_frame(frame)
        self.assertTrue(verify_url in self.marionette.get_url())
        inner_frame_element = self.marionette.get_active_frame()
        # test that we can switch back to main frame, then switch back to the
        # inner frame with the value we got from get_active_frame
        self.marionette.switch_to_frame()
        self.assertEqual(verify_title, self.marionette.title)
        self.marionette.switch_to_frame(inner_frame_element)
        self.assertTrue(verify_url in self.marionette.get_url())
        inner_frame = self.marionette.find_element('id', 'inner_frame')
        self.marionette.switch_to_frame(inner_frame)
        self.assertTrue("test.html" in self.marionette.get_url())
        self.marionette.switch_to_frame() # go back to main frame
        self.assertTrue("test_nested_iframe.html" in self.marionette.get_url())
        #test that we're using the right window object server-side
        self.assertTrue("test_nested_iframe.html" in self.marionette.execute_script("return window.location.href;"))

    def test_stack_trace(self):
        start_url = "test_iframe.html"
        verify_title = "Marionette IFrame Test"
        verify_url = "test.html"
        test_html = self.marionette.absolute_url(start_url)
        self.marionette.navigate(test_html)
        frame = self.marionette.find_element("id", "test_iframe")
        self.assertEqual(self.marionette.get_active_frame(), None)
        self.marionette.switch_to_frame(frame)
        self.assertTrue(verify_url in self.marionette.get_url())
        inner_frame_element = self.marionette.get_active_frame()
        # test that we can switch back to main frame, then switch back to the
        # inner frame with the value we got from get_active_frame
        self.marionette.switch_to_frame()
        self.assertEqual(verify_title, self.marionette.title)
        self.marionette.switch_to_frame(inner_frame_element)
        self.assertTrue(verify_url in self.marionette.get_url())

        #can't use assertRaises in context manager with python2.6
        self.assertRaises(JavascriptException, self.marionette.execute_async_script, "foo();")
        try:
            self.marionette.execute_async_script("foo();")
        except JavascriptException as e:
            self.assertTrue("foo" in e.msg)

    def test_should_be_able_to_carry_on_working_if_the_frame_is_deleted_from_under_us(self):
        test_html = self.marionette.absolute_url("deletingFrame.html")
        self.marionette.navigate(test_html)

        self.marionette.switch_to_frame(self.marionette.find_element('id',
                                                                     'iframe1'))
        killIframe = self.marionette.find_element("id", "killIframe")
        killIframe.click()
        self.marionette.switch_to_frame()

        self.assertEqual(0, len(self.marionette.find_elements("id", "iframe1")))

        addIFrame = self.marionette.find_element("id", "addBackFrame")
        addIFrame.click()
        self.marionette.find_element("id", "iframe1")

        self.marionette.switch_to_frame(self.marionette.find_element("id",
                                                                     "iframe1"))

        self.marionette.find_element("id", "checkbox")

    def test_should_allow_a_user_to_switch_from_an_iframe_back_to_the_main_content_of_the_page(self):
        test_iframe = self.marionette.absolute_url("test_iframe.html")
        self.marionette.navigate(test_iframe)
        self.marionette.switch_to_frame(0)
        self.marionette.switch_to_default_content()
        header = self.marionette.find_element("id", "iframe_page_heading")
        self.assertEqual(header.text, "This is the heading")

    def test_should_be_able_to_switch_to_a_frame_by_its_index(self):
        test_html = self.marionette.absolute_url("frameset.html")
        self.marionette.navigate(test_html)
        self.marionette.switch_to_frame(2)
        element = self.marionette.find_element("id", "email")
        self.assertEquals("email", element.get_attribute("type"))

    def test_should_be_able_to_switch_to_a_frame_using_a_previously_located_element(self):
        test_html = self.marionette.absolute_url("frameset.html")
        self.marionette.navigate(test_html)
        frame = self.marionette.find_element("name", "third")
        self.marionette.switch_to_frame(frame)

        element = self.marionette.find_element("id", "email")
        self.assertEquals("email", element.get_attribute("type"))

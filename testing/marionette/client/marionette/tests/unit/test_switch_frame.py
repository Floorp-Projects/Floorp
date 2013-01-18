# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_test import MarionetteTestCase
from errors import JavascriptException

# boiler plate for the initial navigation and frame switch
def switch_to_window_verify(test, start_url, frame, verify_title, verify_url):
    test.assertTrue(test.marionette.execute_script("window.location.href = 'about:blank'; return true;"))
    test.assertEqual("about:blank", test.marionette.execute_script("return window.location.href;"))
    test_html = test.marionette.absolute_url(start_url)
    test.marionette.navigate(test_html)
    test.assertNotEqual("about:blank", test.marionette.execute_script("return window.location.href;"))
    test.assertEqual(verify_title, test.marionette.title)
    test.marionette.switch_to_frame(frame)
    test.assertTrue(verify_url in test.marionette.get_url())

class TestSwitchFrame(MarionetteTestCase):
    def test_switch_simple(self):
        switch_to_window_verify(self, "test_iframe.html", "test_iframe", "Marionette IFrame Test", "test.html")

    def test_switch_nested(self):
        switch_to_window_verify(self, "test_nested_iframe.html", "test_iframe", "Marionette IFrame Test", "test_inner_iframe.html")
        self.marionette.switch_to_frame("inner_frame")
        self.assertTrue("test.html" in self.marionette.get_url())
        self.marionette.switch_to_frame() # go back to main frame
        self.assertTrue("test_nested_iframe.html" in self.marionette.get_url())
        #test that we're using the right window object server-side
        self.assertTrue("test_nested_iframe.html" in self.marionette.execute_script("return window.location.href;"))

    def test_stack_trace(self):
        switch_to_window_verify(self, "test_iframe.html", "test_iframe", "Marionette IFrame Test", "test.html")
        #can't use assertRaises in context manager with python2.6
        self.assertRaises(JavascriptException, self.marionette.execute_async_script, "foo();")
        try:
            self.marionette.execute_async_script("foo();")
        except JavascriptException as e:
            self.assertTrue("foo" in e.msg)

    def testShouldBeAbleToCarryOnWorkingIfTheFrameIsDeletedFromUnderUs(self):
        test_html = self.marionette.absolute_url("deletingFrame.html")
        self.marionette.navigate(test_html)

        self.marionette.switch_to_frame("iframe1");
        killIframe = self.marionette.find_element("id" ,"killIframe")
        killIframe.click()
        self.marionette.switch_to_frame()

        self.assertEqual(0, len(self.marionette.find_elements("id", "iframe1")))

        addIFrame = self.marionette.find_element("id", "addBackFrame")
        addIFrame.click()
        self.marionette.find_element("id", "iframe1")

        self.marionette.switch_to_frame("iframe1");

        self.marionette.find_element("id", "checkbox")

class TestSwitchFrameChrome(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context("chrome")
        self.win = self.marionette.current_window_handle
        self.marionette.execute_script("window.open('chrome://marionette/content/test.xul', 'foo', 'chrome,centerscreen');")
        self.marionette.switch_to_window('foo')
        self.assertNotEqual(self.win, self.marionette.current_window_handle)

    def tearDown(self):
        self.assertNotEqual(self.win, self.marionette.current_window_handle)
        self.marionette.execute_script("window.close();")
        self.marionette.switch_to_window(self.win)
        MarionetteTestCase.tearDown(self)

    def test_switch_simple(self):
        self.assertTrue("test.xul" in self.marionette.get_url())
        self.marionette.switch_to_frame(0)
        self.assertTrue("test2.xul" in self.marionette.get_url())
        self.marionette.switch_to_frame()
        self.assertTrue("test.xul" in self.marionette.get_url())
        self.marionette.switch_to_frame("iframe")
        self.assertTrue("test2.xul" in self.marionette.get_url())
        self.marionette.switch_to_frame()
        self.assertTrue("test.xul" in self.marionette.get_url())
        self.marionette.switch_to_frame("iframename")
        self.assertTrue("test2.xul" in self.marionette.get_url())
        self.marionette.switch_to_frame()
        self.assertTrue("test.xul" in self.marionette.get_url())
        
    def test_stack_trace(self):
        self.assertTrue("test.xul" in self.marionette.get_url())
        self.marionette.switch_to_frame(0)
        self.assertRaises(JavascriptException, self.marionette.execute_async_script, "foo();")
        try:
            self.marionette.execute_async_script("foo();")
        except JavascriptException as e:
            self.assertTrue("foo" in e.msg)

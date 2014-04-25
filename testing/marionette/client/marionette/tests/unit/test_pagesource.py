# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase

class TestPageSource(MarionetteTestCase):
    def testShouldReturnTheSourceOfAPage(self):
        test_html = self.marionette.absolute_url("testPageSource.html")
        self.marionette.navigate(test_html)
        source = self.marionette.page_source
        self.assertTrue("<html" in source)
        self.assertTrue("PageSource" in source)

    def testShouldReturnAXMLDocumentSource(self):
        test_xml = self.marionette.absolute_url("testPageSource.xml")
        self.marionette.navigate(test_xml)
        source = self.marionette.page_source
        import re
        self.assertEqual(re.sub("\s", "", source), "<xml><foo><bar>baz</bar></foo></xml>")

class TestPageSourceChrome(MarionetteTestCase):
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

    def testShouldReturnXULDetails(self):
        wins = self.marionette.window_handles
        wins.remove(self.win)
        newWin = wins.pop()
        self.marionette.switch_to_window(newWin)
        source  = self.marionette.page_source
        self.assertTrue('<textbox id="textInput"' in source)

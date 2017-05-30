# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.errors import NoSuchWindowException

from marionette_harness import MarionetteTestCase, WindowManagerMixin, skip_if_mobile


class TestGetCurrentUrlChrome(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestGetCurrentUrlChrome, self).setUp()
        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()
        super(TestGetCurrentUrlChrome, self).tearDown()

    def test_browser_window(self):
        url = self.marionette.absolute_url("test.html")

        with self.marionette.using_context("content"):
            self.marionette.navigate(url)
            self.assertEqual(self.marionette.get_url(), url)

        chrome_url = self.marionette.execute_script("return window.location.href;")
        self.assertEqual(self.marionette.get_url(), chrome_url)

    @skip_if_mobile("Fennec doesn't support other chrome windows")
    def test_no_browser_window(self):

        def open_window_with_js():
            with self.marionette.using_context("chrome"):
                self.marionette.execute_script("""
                  window.open('chrome://marionette/content/test.xul',
                              'foo', 'chrome,centerscreen');
                """)

        win = self.open_window(trigger=open_window_with_js)
        self.marionette.switch_to_window(win)

        chrome_url = self.marionette.execute_script("return window.location.href;")
        self.assertEqual(self.marionette.get_url(), chrome_url)

        # With no tabbrowser available an exception will be thrown
        with self.assertRaises(NoSuchWindowException):
            with self.marionette.using_context("content"):
                self.marionette.get_url()

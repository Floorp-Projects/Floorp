# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.errors import UnsupportedOperationException
from marionette import MarionetteTestCase


class TestSetWindowSize(MarionetteTestCase):
    def setUp(self):
        super(MarionetteTestCase, self).setUp()
        self.start_size = self.marionette.window_size
        self.max_width = self.marionette.execute_script("return window.screen.availWidth;")
        self.max_height = self.marionette.execute_script("return window.screen.availHeight;")

    def tearDown(self):
        # WebDriver spec says a resize cannot result in window being maximized, an
        # error is returned if that is the case; therefore if the window is maximized
        # at the start of this test, returning to the original size via set_window_size
        # size will result in error; so reset to original size minus 1 pixel width
        if self.start_size['width'] == self.max_width and self.start_size['height'] == self.max_height:
            self.start_size['width']-=1
        self.marionette.set_window_size(self.start_size['width'], self.start_size['height'])
        super(MarionetteTestCase, self).tearDown()

    def test_that_we_can_get_and_set_window_size(self):
        # event handler
        self.marionette.execute_script("""
        window.wrappedJSObject.rcvd_event = false;
        window.onresize = function() {
            window.wrappedJSObject.rcvd_event = true;
        };
        """)

        # valid size
        width = self.max_width - 100
        height = self.max_height - 100
        self.marionette.set_window_size(width, height)
        self.wait_for_condition(lambda m: m.execute_script("return window.wrappedJSObject.rcvd_event;"))
        size = self.marionette.window_size
        self.assertEqual(size['width'], width,
                         "Window width is %s but should be %s" % (size['width'], width))
        self.assertEqual(size['height'], height,
                         "Window height is %s but should be %s" % (size['height'], height))

    def test_that_we_throw_an_error_when_trying_to_set_maximum_size(self):
        # valid size
        width = self.max_width - 100
        height = self.max_height - 100
        self.marionette.set_window_size(width, height)
        # invalid size (cannot maximize)
        with self.assertRaisesRegexp(UnsupportedOperationException, "Requested size exceeds screen size"):
            self.marionette.set_window_size(self.max_width, self.max_height)
        size = self.marionette.window_size
        self.assertEqual(size['width'], width, "Window width should not have changed")
        self.assertEqual(size['height'], height, "Window height should not have changed")

    def test_that_we_can_maximise_the_window(self):
        # valid size
        width = self.max_width - 100
        height = self.max_height - 100
        self.marionette.set_window_size(width, height)

        # event handler
        self.marionette.execute_script("""
        window.wrappedJSObject.rcvd_event = false;
        window.onresize = function() {
            window.wrappedJSObject.rcvd_event = true;
        };
        """)
        self.marionette.maximize_window()
        self.wait_for_condition(lambda m: m.execute_script("return window.wrappedJSObject.rcvd_event;"))

        size = self.marionette.window_size
        self.assertGreaterEqual(size['width'], self.max_width,
                         "Window width does not use availWidth, current width: {0}, max width: {1}".format(size['width'], self.max_width))
        self.assertGreaterEqual(size['height'], self.max_height,
                         "Window height does not use availHeight. current width: {0}, max width: {1}".format(size['height'], self.max_height))

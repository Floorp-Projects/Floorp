# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness import MarionetteTestCase


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
                         "Window width is {0} but should be {1}".format(size['width'], width))
        self.assertEqual(size['height'], height,
                         "Window height is {0} but should be {1}".format(size['height'], height))

    def test_that_we_can_get_new_size_when_set_window_size(self):
        actual = self.marionette.window_size
        width = actual['width'] - 50
        height = actual['height'] - 50
        size = self.marionette.set_window_size(width, height)
        self.assertIsNotNone(size, "Response is None")
        self.assertEqual(size['width'], width,
                         "New width is {0} but should be {1}".format(size['width'], width))
        self.assertEqual(size['height'], height,
                         "New height is {0} but should be {1}".format(size['height'], height))

    def test_possible_to_request_window_larger_than_screen(self):
        self.marionette.set_window_size(4 * self.max_width, 4 * self.max_height)
        size = self.marionette.window_size

        # In X the window size may be greater than the bounds of the screen
        self.assertGreaterEqual(size["width"], self.max_width)
        self.assertGreaterEqual(size["height"], self.max_height)

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

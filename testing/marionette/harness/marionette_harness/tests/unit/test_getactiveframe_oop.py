# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase


OOP_BY_DEFAULT = "dom.ipc.browser_frames.oop_by_default"
BROWSER_FRAMES_ENABLED = "dom.mozBrowserFramesEnabeld"


class TestGetActiveFrameOOP(MarionetteTestCase):
    def setUp(self):
        super(TestGetActiveFrameOOP, self).setUp()
        with self.marionette.using_context("chrome"):
            self.oop_by_default = self.marionette.get_pref(OOP_BY_DEFAULT)
            self.mozBrowserFramesEnabled = self.marionette.get_pref(BROWSER_FRAMES_ENABLED)
            self.marionette.set_pref(OOP_BY_DEFAULT, True)
            self.marionette.set_pref(BROWSER_FRAMES_ENABLED, True)

    def tearDown(self):
        with self.marionette.using_context("chrome"):
            if self.oop_by_default is None:
                self.marionette.clear_pref(OOP_BY_DEFAULT)
            else:
                self.marionette.set_pref(OOP_BY_DEFAULT, self.oop_by_default)

            if self.mozBrowserFramesEnabled is None:
                self.marionette.clear_pref(BROWSER_FRAMES_ENABLED)
            else:
                self.marionette.set_pref(BROWSER_FRAMES_ENABLED, self.mozBrowserFramesEnabled)

    def test_active_frame_oop(self):
        self.marionette.navigate(self.marionette.absolute_url("test.html"))
        self.marionette.push_permission('browser', True)

        # Create first OOP frame
        self.marionette.execute_script("""
            let iframe1 = document.createElement("iframe");
            iframe1.id = "remote_iframe1";
            iframe1.setAttribute('remote', true);
            iframe1.setAttribute('mozbrowser', true);
            iframe1.style.height = "100px";
            iframe1.style.width = "100%%";
            iframe1.src = "{}";
            document.body.appendChild(iframe1);
            """.format(self.marionette.absolute_url("test_oop_1.html")))

        # Currently no active frame
        self.assertEqual(self.marionette.get_active_frame(), None)
        self.assertTrue("test.html" in self.marionette.get_url())

        # Switch to iframe1, get active frame
        frame = self.marionette.find_element(By.ID, 'remote_iframe1')
        self.marionette.switch_to_frame(frame)
        active_frame1 = self.marionette.get_active_frame()
        self.assertNotEqual(active_frame1.id, None)

        # Switch to top-level then back to active frame, verify correct frame
        self.marionette.switch_to_frame()
        self.marionette.switch_to_frame(active_frame1)
        self.assertTrue("test_oop_1.html" in self.marionette.execute_script("return document.wrappedJSObject.location.href"))

        # Create another OOP frame
        self.marionette.switch_to_frame()
        self.marionette.execute_script("""
            let iframe2 = document.createElement("iframe");
            iframe2.setAttribute('mozbrowser', true);
            iframe2.setAttribute('remote', true);
            iframe2.id = "remote_iframe2";
            iframe2.style.height = "100px";
            iframe2.style.width = "100%%";
            iframe2.src = "{}";
            document.body.appendChild(iframe2);
            """.format(self.marionette.absolute_url("test_oop_2.html")))

        # Switch to iframe2, get active frame
        frame2 = self.marionette.find_element(By.ID, 'remote_iframe2')
        self.marionette.switch_to_frame(frame2)
        active_frame2 = self.marionette.get_active_frame()
        self.assertNotEqual(active_frame2.id, None)

        # Switch to top-level then back to active frame 1, verify correct frame
        self.marionette.switch_to_frame()
        self.marionette.switch_to_frame(active_frame1)
        self.assertTrue("test_oop_1.html" in self.marionette.execute_script("return document.wrappedJSObject.location.href"))

        # Switch to top-level then back to active frame 2, verify correct frame
        self.marionette.switch_to_frame()
        self.marionette.switch_to_frame(active_frame2)
        self.assertTrue("test_oop_2.html" in self.marionette.execute_script("return document.wrappedJSObject.location.href"))

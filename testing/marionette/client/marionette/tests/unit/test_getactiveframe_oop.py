# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase


class TestGetActiveFrameOOP(MarionetteTestCase):
    def setUp(self):
        super(TestGetActiveFrameOOP, self).setUp()
        self.oop_by_default = self.marionette.execute_script("""
            try {
              return SpecialPowers.getBoolPref('dom.ipc.browser_frames.oop_by_default');
            }
            catch(e) {}
            """)
        self.mozBrowserFramesEnabled = self.marionette.execute_script("""
            try {
              return SpecialPowers.getBoolPref('dom.mozBrowserFramesEnabled');
            }
            catch(e) {}
            """)
        self.marionette.execute_script("""
            SpecialPowers.setBoolPref('dom.ipc.browser_frames.oop_by_default', true);
            """)
        self.marionette.execute_script("""
            SpecialPowers.setBoolPref('dom.mozBrowserFramesEnabled', true);
            """)

    def test_active_frame_oop(self):
        self.marionette.navigate(self.marionette.absolute_url("test.html"))
        self.marionette.push_permission('browser', True)

        # Create first OOP frame
        self.marionette.execute_script("""
            let iframe1 = document.createElement("iframe");
            SpecialPowers.wrap(iframe1).mozbrowser = true;
            SpecialPowers.wrap(iframe1).remote = true;
            iframe1.id = "remote_iframe1";
            iframe1.style.height = "100px";
            iframe1.style.width = "100%%";
            iframe1.src = "%s";
            document.body.appendChild(iframe1);
            """ % self.marionette.absolute_url("test_oop_1.html"))

        # Currently no active frame
        self.assertEqual(self.marionette.get_active_frame(), None)
        self.assertTrue("test.html" in self.marionette.get_url())

        # Switch to iframe1, get active frame
        self.marionette.switch_to_frame('remote_iframe1')
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
            SpecialPowers.wrap(iframe2).mozbrowser = true;
            SpecialPowers.wrap(iframe2).remote = true;
            iframe2.id = "remote_iframe2";
            iframe2.style.height = "100px";
            iframe2.style.width = "100%%";
            iframe2.src = "%s";
            document.body.appendChild(iframe2);
            """ % self.marionette.absolute_url("test_oop_2.html"))

        # Switch to iframe2, get active frame
        self.marionette.switch_to_frame('remote_iframe2')
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

        # NOTE: For some reason the emulator, the contents of the OOP iframes are not
        # actually rendered, even though the page_source is correct. When this test runs
        # on a b2g device, the contents do appear
        # print self.marionette.get_url()
        # print self.marionette.page_source

    def tearDown(self):
        if self.oop_by_default is None:
            self.marionette.execute_script("""
                SpecialPowers.clearUserPref('dom.ipc.browser_frames.oop_by_default');
                """)
        else:
            self.marionette.execute_script("""
                SpecialPowers.setBoolPref('dom.ipc.browser_frames.oop_by_default', %s);
                """ % 'true' if self.oop_by_default else 'false')
        if self.mozBrowserFramesEnabled is None:
            self.marionette.execute_script("""
                SpecialPowers.clearUserPref('dom.mozBrowserFramesEnabled');
                """)
        else:
            self.marionette.execute_script("""
                SpecialPowers.setBoolPref('dom.mozBrowserFramesEnabled', %s);
                """ % 'true' if self.mozBrowserFramesEnabled else 'false')

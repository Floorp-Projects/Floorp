# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase


OOP_BY_DEFAULT = "dom.ipc.browser_frames.oop_by_default"
BROWSER_FRAMES_ENABLED = "dom.mozBrowserFramesEnabled"


class TestSwitchRemoteFrame(MarionetteTestCase):
    def setUp(self):
        super(TestSwitchRemoteFrame, self).setUp()
        with self.marionette.using_context('chrome'):
            self.oop_by_default = self.marionette.get_pref(OOP_BY_DEFAULT)
            self.mozBrowserFramesEnabled = self.marionette.get_pref(BROWSER_FRAMES_ENABLED)
            self.marionette.set_pref(OOP_BY_DEFAULT, True)
            self.marionette.set_pref(BROWSER_FRAMES_ENABLED, True)

            self.multi_process_browser = self.marionette.execute_script("""
                try {
                  return Services.appinfo.browserTabsRemoteAutostart;
                } catch (e) {
                  return false;
                }""")

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

    @property
    def is_main_process(self):
        return self.marionette.execute_script("""
            return Components.classes["@mozilla.org/xre/app-info;1"].
                getService(Components.interfaces.nsIXULRuntime).
                processType == Components.interfaces.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
        """, sandbox="system")

    def test_remote_frame(self):
        self.marionette.navigate(self.marionette.absolute_url("test.html"))
        self.marionette.push_permission('browser', True)
        self.marionette.execute_script("""
            let iframe = document.createElement("iframe");
            iframe.setAttribute('mozbrowser', true);
            iframe.setAttribute('remote', true);
            iframe.id = "remote_iframe";
            iframe.style.height = "100px";
            iframe.style.width = "100%%";
            iframe.src = "{}";
            document.body.appendChild(iframe);
            """.format(self.marionette.absolute_url("test.html")))
        remote_iframe = self.marionette.find_element(By.ID, "remote_iframe")
        self.marionette.switch_to_frame(remote_iframe)
        main_process = self.is_main_process
        self.assertFalse(main_process)

    def test_remote_frame_revisit(self):
        # test if we can revisit a remote frame (this takes a different codepath)
        self.marionette.navigate(self.marionette.absolute_url("test.html"))
        self.marionette.push_permission('browser', True)
        self.marionette.execute_script("""
            let iframe = document.createElement("iframe");
            iframe.setAttribute('mozbrowser', true);
            iframe.setAttribute('remote', true);
            iframe.id = "remote_iframe";
            iframe.style.height = "100px";
            iframe.style.width = "100%%";
            iframe.src = "{}";
            document.body.appendChild(iframe);
            """.format(self.marionette.absolute_url("test.html")))
        self.marionette.switch_to_frame(self.marionette.find_element(By.ID,
                                                                     "remote_iframe"))
        main_process = self.is_main_process
        self.assertFalse(main_process)
        self.marionette.switch_to_frame()
        main_process = self.is_main_process
        should_be_main_process = not self.multi_process_browser
        self.assertEqual(main_process, should_be_main_process)
        self.marionette.switch_to_frame(self.marionette.find_element(By.ID,
                                                                     "remote_iframe"))
        main_process = self.is_main_process
        self.assertFalse(main_process)

    def test_we_can_switch_to_a_remote_frame_by_index(self):
        # test if we can revisit a remote frame (this takes a different codepath)
        self.marionette.navigate(self.marionette.absolute_url("test.html"))
        self.marionette.push_permission('browser', True)
        self.marionette.execute_script("""
            let iframe = document.createElement("iframe");
            iframe.setAttribute('mozbrowser', true);
            iframe.setAttribute('remote', true);
            iframe.id = "remote_iframe";
            iframe.style.height = "100px";
            iframe.style.width = "100%%";
            iframe.src = "{}";
            document.body.appendChild(iframe);
            """.format(self.marionette.absolute_url("test.html")))
        self.marionette.switch_to_frame(0)
        main_process = self.is_main_process
        self.assertFalse(main_process)
        self.marionette.switch_to_frame()
        main_process = self.is_main_process
        should_be_main_process = not self.multi_process_browser
        self.assertEqual(main_process, should_be_main_process)
        self.marionette.switch_to_frame(0)
        main_process = self.is_main_process
        self.assertFalse(main_process)

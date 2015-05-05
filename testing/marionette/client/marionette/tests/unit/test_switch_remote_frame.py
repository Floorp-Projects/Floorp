# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase


class TestSwitchRemoteFrame(MarionetteTestCase):
    def setUp(self):
        super(TestSwitchRemoteFrame, self).setUp()
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
        self.marionette.execute_async_script(
            'SpecialPowers.pushPrefEnv({"set": [["dom.ipc.browser_frames.oop_by_default", true], ["dom.mozBrowserFramesEnabled", true]]}, marionetteScriptFinished);')

        with self.marionette.using_context('chrome'):
            self.multi_process_browser = self.marionette.execute_script("""
                try {
                  return Services.appinfo.browserTabsRemoteAutostart;
                } catch (e) {
                  return false;
                }""")

    def test_remote_frame(self):
        self.marionette.navigate(self.marionette.absolute_url("test.html"))
        self.marionette.push_permission('browser', True)
        self.marionette.execute_script("""
            let iframe = document.createElement("iframe");
            SpecialPowers.wrap(iframe).mozbrowser = true;
            SpecialPowers.wrap(iframe).remote = true;
            iframe.id = "remote_iframe";
            iframe.style.height = "100px";
            iframe.style.width = "100%%";
            iframe.src = "%s";
            document.body.appendChild(iframe);
            """ % self.marionette.absolute_url("test.html"))
        remote_iframe = self.marionette.find_element("id", "remote_iframe")
        self.marionette.switch_to_frame(remote_iframe)
        main_process = self.marionette.execute_script("""
            return SpecialPowers.isMainProcess();
            """)
        self.assertFalse(main_process)

    def test_remote_frame_revisit(self):
        # test if we can revisit a remote frame (this takes a different codepath)
        self.marionette.navigate(self.marionette.absolute_url("test.html"))
        self.marionette.push_permission('browser', True)
        self.marionette.execute_script("""
            let iframe = document.createElement("iframe");
            SpecialPowers.wrap(iframe).mozbrowser = true;
            SpecialPowers.wrap(iframe).remote = true;
            iframe.id = "remote_iframe";
            iframe.style.height = "100px";
            iframe.style.width = "100%%";
            iframe.src = "%s";
            document.body.appendChild(iframe);
            """ % self.marionette.absolute_url("test.html"))
        self.marionette.switch_to_frame(self.marionette.find_element("id",
                                                                     "remote_iframe"))
        main_process = self.marionette.execute_script("""
            return SpecialPowers.isMainProcess();
            """)
        self.assertFalse(main_process)
        self.marionette.switch_to_frame()
        main_process = self.marionette.execute_script("""
            return SpecialPowers.isMainProcess();
            """)
        should_be_main_process = not self.multi_process_browser
        self.assertEqual(main_process, should_be_main_process)
        self.marionette.switch_to_frame(self.marionette.find_element("id",
                                                                     "remote_iframe"))
        main_process = self.marionette.execute_script("""
            return SpecialPowers.isMainProcess();
            """)
        self.assertFalse(main_process)

    def test_we_can_switch_to_a_remote_frame_by_index(self):
        # test if we can revisit a remote frame (this takes a different codepath)
        self.marionette.navigate(self.marionette.absolute_url("test.html"))
        self.marionette.push_permission('browser', True)
        self.marionette.execute_script("""
            let iframe = document.createElement("iframe");
            SpecialPowers.wrap(iframe).mozbrowser = true;
            SpecialPowers.wrap(iframe).remote = true;
            iframe.id = "remote_iframe";
            iframe.style.height = "100px";
            iframe.style.width = "100%%";
            iframe.src = "%s";
            document.body.appendChild(iframe);
            """ % self.marionette.absolute_url("test.html"))
        self.marionette.switch_to_frame(0)
        main_process = self.marionette.execute_script("""
            return SpecialPowers.isMainProcess();
            """)
        self.assertFalse(main_process)
        self.marionette.switch_to_frame()
        main_process = self.marionette.execute_script("""
            return SpecialPowers.isMainProcess();
            """)
        should_be_main_process = not self.multi_process_browser
        self.assertEqual(main_process, should_be_main_process)
        self.marionette.switch_to_frame(0)
        main_process = self.marionette.execute_script("""
            return SpecialPowers.isMainProcess();
            """)
        self.assertFalse(main_process)

    def tearDown(self):
        if self.oop_by_default is None:
            self.marionette.execute_script("""
                SpecialPowers.clearUserPref('dom.ipc.browser_frames.oop_by_default');
                """)
        else:
             self.marionette.execute_async_script(
            'SpecialPowers.pushPrefEnv({"set": [["dom.ipc.browser_frames.oop_by_default", %s]]}, marionetteScriptFinished);' %
            ('true' if self.oop_by_default else 'false'))
        if self.mozBrowserFramesEnabled is None:
            self.marionette.execute_script("""
                SpecialPowers.clearUserPref('dom.mozBrowserFramesEnabled');
                """)
        else:
            self.marionette.execute_async_script(
            'SpecialPowers.pushPrefEnv({"set": [["dom.mozBrowserFramesEnabled", %s]]}, marionetteScriptFinished);' %
            ('true' if self.mozBrowserFramesEnabled else 'false'))

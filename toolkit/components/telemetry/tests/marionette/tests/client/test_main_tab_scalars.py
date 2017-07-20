# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.wait import Wait

from telemetry_harness.testcase import TelemetryTestCase


class TestMainTabScalars(TelemetryTestCase):

    def test_main_tab_scalars(self):
        wait = Wait(self.marionette, 10)

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            tab1 = self.browser.tabbar.selected_tab
            tab2 = self.browser.tabbar.open_tab()
            wait.until(lambda m: len(self.browser.tabbar.tabs) == 2)
            self.browser.tabbar.switch_to(tab2)
            tab3 = self.browser.tabbar.open_tab()
            wait.until(lambda m: len(self.browser.tabbar.tabs) == 3)
            self.browser.tabbar.switch_to(tab3)
            self.browser.tabbar.close_tab(tab3, force=True)
            wait.until(lambda m: len(self.browser.tabbar.tabs) == 2)
            self.browser.tabbar.close_tab(tab2, force=True)
            wait.until(lambda m: len(self.browser.tabbar.tabs) == 1)
            self.browser.tabbar.switch_to(tab1)
        self.restart_browser()
        ping = self.wait_for_ping(lambda p: p['type'] == 'main'
                                  and p['payload']['info']['reason'] == 'shutdown')
        assert ping['type'] == 'main'
        assert ping['clientId'] == self.client_id
        scalars = ping['payload']['processes']['parent']['scalars']
        assert scalars['browser.engagement.max_concurrent_tab_count'] == 3
        assert scalars['browser.engagement.tab_open_event_count'] == 2
        assert scalars['browser.engagement.max_concurrent_window_count'] == 1

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from telemetry_harness.testcase import TelemetryTestCase


class TestMainTabScalars(TelemetryTestCase):

    def test_main_tab_scalars(self):
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            tab2 = self.browser.tabbar.open_tab()
            self.browser.tabbar.switch_to(tab2)
            tab3 = self.browser.tabbar.open_tab()
            self.browser.tabbar.switch_to(tab3)
            self.browser.tabbar.close_tab(tab3, force=True)
            self.browser.tabbar.close_tab(tab2, force=True)
        self.install_addon()
        ping = self.wait_for_ping()
        assert ping['type'] == 'main'
        assert ping['clientId'] == self.client_id
        scalars = ping['payload']['processes']['parent']['scalars']
        assert scalars['browser.engagement.max_concurrent_tab_count'] == 3
        assert scalars['browser.engagement.tab_open_event_count'] == 2
        assert scalars['browser.engagement.max_concurrent_window_count'] == 1

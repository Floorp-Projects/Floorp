# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from marionette_driver import By
from marionette_driver.errors import TimeoutException

from firefox_ui_harness.testcases import FirefoxTestCase
from firefox_puppeteer.ui.browser.notifications import (
    AddOnInstallFailedNotification)

here = os.path.abspath(os.path.dirname(__file__))


class TestNotifications(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)
        # setting this pref prevents a notification from being displayed
        # when e10s is enabled. see http://mzl.la/1TVNAXh
        self.prefs.set_pref('browser.tabs.remote.force-enable', True)

    def tearDown(self):
        try:
            if self.browser.notification is not None:
                self.browser.notification.close()
        finally:
            FirefoxTestCase.tearDown(self)

    def test_no_notification(self):
        """No initial notifications are shown"""
        self.assertIsNone(self.browser.notification)

    def test_notification_with_origin(self):
        """Trigger a notification with an origin"""
        self.trigger_add_on_notification('mn-restartless-unsigned.xpi')
        self.assertIsNotNone(self.browser.notification.origin)
        self.assertIsNotNone(self.browser.notification.label)

    def test_close_notification(self):
        """Trigger and dismiss a notification"""
        self.trigger_add_on_notification('mn-restartless-unsigned.xpi')
        self.browser.notification.close()
        self.assertIsNone(self.browser.notification)

    def test_add_on_failed_notification(self):
        """Trigger add-on failed notification using an unsigned add-on"""
        self.trigger_add_on_notification('mn-restartless-unsigned.xpi')
        self.assertIsInstance(self.browser.notification,
                              AddOnInstallFailedNotification)

    def test_wait_for_any_notification_timeout(self):
        """Wait for a notification when one is not shown"""
        message = 'No notification was shown'
        with self.assertRaisesRegexp(TimeoutException, message):
            self.browser.wait_for_notification()

    def test_wait_for_specific_notification_timeout(self):
        """Wait for a notification when one is not shown"""
        message = 'AddOnInstallFailedNotification was not shown'
        with self.assertRaisesRegexp(TimeoutException, message):
            self.browser.wait_for_notification(AddOnInstallFailedNotification)

    def test_wait_for_no_notification_timeout(self):
        """Wait for no notification when one is shown"""
        message = 'Unexpected notification shown'
        self.trigger_add_on_notification('mn-restartless-unsigned.xpi')
        with self.assertRaisesRegexp(TimeoutException, message):
            self.browser.wait_for_notification(None)

    def trigger_add_on_notification(self, add_on):
        with self.marionette.using_context('content'):
            self.marionette.navigate('file://{0}'.format(here))
            self.marionette.find_element(By.LINK_TEXT, add_on).click()
        self.browser.wait_for_notification()

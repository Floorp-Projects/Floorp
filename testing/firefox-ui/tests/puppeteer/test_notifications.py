# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By
from marionette_driver.errors import TimeoutException

from firefox_ui_harness.testcases import FirefoxTestCase
from firefox_puppeteer.ui.browser.notifications import (
    AddOnInstallFailedNotification,
    AddOnInstallConfirmationNotification
)


class TestNotifications(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)

        self.puppeteer.prefs.set_pref('extensions.install.requireSecureOrigin', False)

        self.addons_url = self.marionette.absolute_url('addons/extensions/')
        self.puppeteer.utils.permissions.add(self.marionette.baseurl, 'install')

    def tearDown(self):
        try:
            self.marionette.clear_pref('extensions.install.requireSecureOrigin')
            self.marionette.clear_pref('xpinstall.signatures.required')

            self.puppeteer.utils.permissions.remove(self.addons_url, 'install')

            if self.browser.notification:
                self.browser.notification.close(force=True)
        finally:
            FirefoxTestCase.tearDown(self)

    def test_open_close_notification(self):
        """Trigger and dismiss a notification"""
        self.assertIsNone(self.browser.notification)
        self.trigger_addon_notification('restartless_addon_signed.xpi')
        self.browser.notification.close()
        self.assertIsNone(self.browser.notification)

    def test_wait_for_notification_timeout(self):
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
        self.trigger_addon_notification('restartless_addon_signed.xpi')
        with self.assertRaisesRegexp(TimeoutException, message):
            self.browser.wait_for_notification(None)

    def test_notification_with_origin(self):
        """Trigger a notification with an origin"""
        self.trigger_addon_notification('restartless_addon_signed.xpi')
        self.assertIn(self.browser.notification.origin, self.marionette.baseurl)
        self.assertIsNotNone(self.browser.notification.label)

    def test_addon_install_failed_notification(self):
        """Trigger add-on blocked notification using an unsigned add-on"""
        # Ensure that installing unsigned extensions will fail
        self.puppeteer.prefs.set_pref('xpinstall.signatures.required', True)

        self.trigger_addon_notification(
            'restartless_addon_unsigned.xpi',
            notification=AddOnInstallFailedNotification)

    def trigger_addon_notification(self, addon, notification=AddOnInstallConfirmationNotification):
        with self.marionette.using_context('content'):
            self.marionette.navigate(self.addons_url)
            self.marionette.find_element(By.LINK_TEXT, addon).click()
        self.browser.wait_for_notification(notification)

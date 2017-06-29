# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from firefox_puppeteer.ui.browser.notifications import (
    AddOnInstallConfirmationNotification,
    AddOnInstallFailedNotification,
)
from marionette_driver import By
from marionette_driver.errors import TimeoutException
from marionette_harness import MarionetteTestCase


class TestNotifications(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestNotifications, self).setUp()

        self.marionette.set_pref('extensions.install.requireSecureOrigin', False)
        self.marionette.set_pref('extensions.allow-non-mpc-extensions', True)

        self.addons_url = self.marionette.absolute_url('addons/extensions/')
        self.puppeteer.utils.permissions.add(self.marionette.baseurl, 'install')

    def tearDown(self):
        try:
            self.marionette.clear_pref('extensions.install.requireSecureOrigin')
            self.marionette.clear_pref('extensions.allow-non-mpc-extensions')
            self.marionette.clear_pref('xpinstall.signatures.required')

            self.puppeteer.utils.permissions.remove(self.addons_url, 'install')

            if self.browser.notification:
                self.browser.notification.close(force=True)
        finally:
            super(TestNotifications, self).tearDown()

    def test_open_close_notification(self):
        """Trigger and dismiss a notification"""
        self.assertIsNone(self.browser.notification)
        self.trigger_addon_notification('webextension-signed.xpi')
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
        self.trigger_addon_notification('webextension-signed.xpi')
        with self.assertRaisesRegexp(TimeoutException, message):
            self.browser.wait_for_notification(None)

    def test_notification_with_origin(self):
        """Trigger a notification with an origin"""
        self.trigger_addon_notification('webextension-signed.xpi')
        self.assertIn(self.browser.notification.origin, self.marionette.baseurl)
        self.assertIsNotNone(self.browser.notification.label)

    def test_addon_install_failed_notification(self):
        """Trigger add-on blocked notification using an unsigned add-on"""
        # Ensure that installing unsigned extensions will fail
        self.marionette.set_pref('xpinstall.signatures.required', True)

        self.trigger_addon_notification(
            'webextension-unsigned.xpi',
            notification=AddOnInstallFailedNotification)

    def trigger_addon_notification(self, addon, notification=AddOnInstallConfirmationNotification):
        with self.marionette.using_context('content'):
            self.marionette.navigate(self.addons_url)
            self.marionette.find_element(By.LINK_TEXT, addon).click()
        self.browser.wait_for_notification(notification)

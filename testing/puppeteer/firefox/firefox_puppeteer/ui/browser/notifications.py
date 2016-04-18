# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from abc import ABCMeta

from marionette_driver import By

from firefox_puppeteer.ui_base_lib import UIBaseLib


class BaseNotification(UIBaseLib):
    """Abstract base class for any kind of notification."""
    __metaclass__ = ABCMeta

    @property
    def label(self):
        """Provides access to the notification label.

        :returns: The notification label.
        """
        return self.element.get_attribute('label')

    @property
    def origin(self):
        """Provides access to the notification origin.

        :returns: The notification origin.
        """
        return self.element.get_attribute('origin')

    def close(self):
        """Close the notification."""
        self.element.find_element(
            By.ANON_ATTRIBUTE, {'anonid': 'closebutton'}).click()


class AddOnInstallBlockedNotification(BaseNotification):
    """Add-on install blocked notification."""

    def allow(self):
        """Allow the add-on to be installed."""
        self.element.find_element(
            By.ANON_ATTRIBUTE, {'anonid': 'button'}).find_element(
            By.ANON_ATTRIBUTE, {'anonid': 'button'}).click()


class AddOnInstallConfirmationNotification(BaseNotification):
    """Add-on install confirmation notification."""

    @property
    def add_on(self):
        """Provides access to the add-on name.

        :returns: The add-on name.
        """
        label = self.element.find_element(
            By.CSS_SELECTOR, '#addon-install-confirmation-content label')
        return label.get_attribute('value')

    def cancel(self):
        """Cancel installation of the add-on."""
        self.element.find_element(
            By.ID, 'addon-install-confirmation-cancel').click()

    def install(self):
        """Proceed with installation of the add-on."""
        self.element.find_element(
            By.ID, 'addon-install-confirmation-accept').click()


class AddOnInstallCompleteNotification(BaseNotification):
    """Add-on install complete notification."""
    pass


class AddOnInstallFailedNotification(BaseNotification):
    """Add-on install failed notification."""
    pass


class AddOnProgressNotification(BaseNotification):
    """Add-on progress notification."""
    pass

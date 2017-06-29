# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from abc import ABCMeta

from marionette_driver import By

from firefox_puppeteer.ui.base import UIBaseLib


class BaseNotification(UIBaseLib):
    """Abstract base class for any kind of notification."""

    __metaclass__ = ABCMeta

    @property
    def close_button(self):
        """Provide access to the close button.

        :returns: The close button.
        """
        return self.element.find_element(By.ANON_ATTRIBUTE,
                                         {'anonid': 'closebutton'})

    @property
    def label(self):
        """Provide access to the notification label.

        :returns: The notification label.
        """
        return self.element.get_attribute('label')

    @property
    def origin(self):
        """Provide access to the notification origin.

        :returns: The notification origin.
        """
        return self.element.get_attribute('origin')

    def close(self, force=False):
        """Close the notification.

        :param force: Optional, if True force close the notification.
         Defaults to False.
        """
        if force:
            self.marionette.execute_script('arguments[0].click()',
                                           script_args=[self.close_button])
        else:
            self.close_button.click()

        self.window.wait_for_notification(None)


class AddOnInstallBlockedNotification(BaseNotification):
    """Add-on install blocked notification."""

    @property
    def allow_button(self):
        """Provide access to the allow button.

        :returns: The allow button.
        """
        return self.element.find_element(
            By.ANON_ATTRIBUTE, {'anonid': 'button'}).find_element(
            By.ANON_ATTRIBUTE, {'anonid': 'button'})


class AddOnInstallConfirmationNotification(BaseNotification):
    """Add-on install confirmation notification."""

    pass


class AddOnInstallCompleteNotification(BaseNotification):
    """Add-on install complete notification."""

    pass


class AddOnInstallFailedNotification(BaseNotification):
    """Add-on install failed notification."""

    pass


class AddOnProgressNotification(BaseNotification):
    """Add-on progress notification."""

    pass

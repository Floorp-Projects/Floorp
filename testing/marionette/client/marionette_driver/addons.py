# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from . import errors

__all__ = ["Addons", "AddonInstallException"]


class AddonInstallException(errors.MarionetteException):
    pass


class Addons(object):
    """An API for installing and inspecting addons during Gecko
    runtime. This is a partially implemented wrapper around Gecko's
    `AddonManager API`_.

    For example::

        from marionette_driver.addons import Addons
        addons = Addons(marionette)
        addons.install("/path/to/extension.xpi")

    .. _AddonManager API: https://developer.mozilla.org/en-US/Add-ons/Add-on_Manager

    """
    def __init__(self, marionette):
        self._mn = marionette

    def install(self, path, temp=False):
        """Install a Firefox addon.

        If the addon is restartless, it can be used right away. Otherwise
        a restart using :func:`~marionette_driver.marionette.Marionette.restart`
        will be needed.

        :param path: A file path to the extension to be installed.
        :param temp: Install a temporary addon. Temporary addons will
                     automatically be uninstalled on shutdown and do not need
                     to be signed, though they must be restartless.

        :returns: The addon ID string of the newly installed addon.

        :raises: :exc:`AddonInstallException`

        """
        body = {"path": path, "temporary": temp}
        try:
            return self._mn._send_message("addon:install", body, key="value")
        except errors.UnknownException as e:
            raise AddonInstallException(e)

    def uninstall(self, addon_id):
        """Uninstall a Firefox addon.

        If the addon is restartless, it will be uninstalled right away.
        Otherwise a restart using :func:`~marionette_driver.marionette.Marionette.restart`
        will be needed.

        If the call to uninstall is resulting in a `ScriptTimeoutException`,
        an invalid ID is likely being passed in. Unfortunately due to
        AddonManager's implementation, it's hard to retrieve this error from
        Python.

        :param addon_id: The addon ID string to uninstall.

        """
        body = {"id": addon_id}
        self._mn._send_message("addon:uninstall", body)

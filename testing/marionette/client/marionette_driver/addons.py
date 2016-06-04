# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from .errors import MarionetteException

__all__ = ['Addons', 'AddonInstallException']


# From https://developer.mozilla.org/en-US/Add-ons/Add-on_Manager/AddonManager#AddonInstall_errors
ADDON_INSTALL_ERRORS = {
    -1: "ERROR_NETWORK_FAILURE: A network error occured.",
    -2: "ERROR_INCORECT_HASH: The downloaded file did not match the expected hash.",
    -3: "ERROR_CORRUPT_FILE: The file appears to be corrupt.",
    -4: "ERROR_FILE_ACCESS: There was an error accessing the filesystem.",
    -5: "ERROR_SIGNEDSTATE_REQUIRED: The addon must be signed and isn't.",
}


class AddonInstallException(MarionetteException):
    pass


class Addons(object):
    """
    An API for installing and inspecting addons during Gecko runtime. This
    is a partially implemented wrapper around Gecko's `AddonManager API`_.

    For example::

        from marionette_driver.addons import Addons
        addons = Addons(marionette)
        addons.install('path/to/extension.xpi')

    .. _AddonManager API: https://developer.mozilla.org/en-US/Add-ons/Add-on_Manager
    """

    def __init__(self, marionette):
        self._mn = marionette

    def install(self, path, temp=False):
        """Install an addon.

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
        with self._mn.using_context('chrome'):
            addon_id, status = self._mn.execute_async_script("""
              let fileUtils = Components.utils.import("resource://gre/modules/FileUtils.jsm");
              let FileUtils = fileUtils.FileUtils;
              Components.utils.import("resource://gre/modules/AddonManager.jsm");
              let listener = {
                onInstallEnded: function(install, addon) {
                  marionetteScriptFinished([addon.id, 0]);
                },

                onInstallFailed: function(install) {
                  marionetteScriptFinished([null, install.error]);
                },

                onInstalled: function(addon) {
                  AddonManager.removeAddonListener(listener);
                  marionetteScriptFinished([addon.id, 0]);
                }
              }

              let file = new FileUtils.File(arguments[0]);
              let temp = arguments[1];

              if (!temp) {
                AddonManager.getInstallForFile(file, function(aInstall) {
                  if (aInstall.error != 0) {
                    marionetteScriptFinished([null, aInstall.error]);
                  }
                  aInstall.addListener(listener);
                  aInstall.install();
                });
              } else {
                AddonManager.addAddonListener(listener);
                AddonManager.installTemporaryAddon(file);
              }
            """, script_args=[path, temp], debug_script=True)

        if status:
            if status in ADDON_INSTALL_ERRORS:
                raise AddonInstallException(ADDON_INSTALL_ERRORS[status])
            raise AddonInstallException("Addon failed to install with return code: %d" % status)
        return addon_id

    def uninstall(self, addon_id):
        """Uninstall an addon.

        If the addon is restartless, it will be uninstalled right away.
        Otherwise a restart using :func:`~marionette_driver.marionette.Marionette.restart`
        will be needed.

        If the call to uninstall is resulting in a `ScriptTimeoutException`,
        an invalid ID is likely being passed in. Unfortunately due to
        AddonManager's implementation, it's hard to retrieve this error from
        Python.

        :param addon_id: The addon ID string to uninstall.
        """
        with self._mn.using_context('chrome'):
            return self._mn.execute_async_script("""
              Components.utils.import("resource://gre/modules/AddonManager.jsm");
              AddonManager.getAddonByID(arguments[0], function(addon) {
                addon.uninstall();
                marionetteScriptFinished(0);
              });
            """, script_args=[addon_id])

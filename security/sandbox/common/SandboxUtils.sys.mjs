/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

export var SandboxUtils = {
  /**
   * Show a notification bar if user is running without unprivileged namespace
   *
   * @param {NotificationBox} aNotificationBox
   *        The target notification box where notification will be added
   */
  maybeWarnAboutMissingUserNamespaces:
    function SU_maybeWarnAboutMissingUserNamespaces(aNotificationBox) {
      if (AppConstants.platform !== "linux") {
        return;
      }

      // This would cover Flatpak, Snap or any "Packaged App" (e.g., Debian package)
      // Showing the notification on Flatpak would not be correct because of
      // existing Flatpak isolation (see Bug 1882881). And for Snap and
      // Debian packages it would be irrelevant as well.
      const isPackagedApp = Services.sysinfo.getPropertyAsBool("isPackagedApp");
      if (isPackagedApp) {
        return;
      }

      const kSandboxUserNamespacesPref =
        "security.sandbox.warn_unprivileged_namespaces";
      const kSandboxUserNamespacesPrefValue = Services.prefs.getBoolPref(
        kSandboxUserNamespacesPref
      );
      if (!kSandboxUserNamespacesPrefValue) {
        return;
      }

      const userNamespaces =
        Services.sysinfo.getPropertyAsBool("hasUserNamespaces");
      if (userNamespaces) {
        return;
      }

      const mozXulElement = aNotificationBox.stack.ownerGlobal.MozXULElement;
      mozXulElement.insertFTLIfNeeded("toolkit/updates/elevation.ftl");

      let buttons = [
        {
          supportPage: "install-firefox-linux",
          "l10n-id": "sandbox-unprivileged-namespaces-howtofix",
        },
        {
          "l10n-id": "sandbox-unprivileged-namespaces-dismiss-button",
          callback: () => {
            Services.prefs.setBoolPref(kSandboxUserNamespacesPref, false);
          },
        },
      ];

      // Now actually create the notification
      aNotificationBox.appendNotification(
        "sandbox-unprivileged-namespaces",
        {
          label: { "l10n-id": "sandbox-missing-unprivileged-namespaces" },
          priority: aNotificationBox.PRIORITY_WARNING_HIGH,
        },
        buttons
      );
    },
};

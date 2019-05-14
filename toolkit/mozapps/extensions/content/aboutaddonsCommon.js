/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

"use strict";

/* exported attachUpdateHandler, getBrowserElement, openOptionsInTab */

var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
var {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyPreferenceGetter(
  this, "WEBEXT_PERMISSION_PROMPTS",
  "extensions.webextPermissionPrompts", false);

ChromeUtils.defineModuleGetter(this, "Extension",
                               "resource://gre/modules/Extension.jsm");

function getBrowserElement() {
  return window.docShell.chromeEventHandler;
}

function attachUpdateHandler(install) {
  if (!WEBEXT_PERMISSION_PROMPTS) {
    return;
  }

  install.promptHandler = (info) => {
    let oldPerms = info.existingAddon.userPermissions;
    if (!oldPerms) {
      // Updating from a legacy add-on, let it proceed
      return Promise.resolve();
    }

    let newPerms = info.addon.userPermissions;

    let difference = Extension.comparePermissions(oldPerms, newPerms);

    // If there are no new permissions, just proceed
    if (difference.origins.length == 0 && difference.permissions.length == 0) {
      return Promise.resolve();
    }

    return new Promise((resolve, reject) => {
      let subject = {
        wrappedJSObject: {
          target: getBrowserElement(),
          info: {
            type: "update",
            addon: info.addon,
            icon: info.addon.icon,
            // Reference to the related AddonInstall object (used in
            // AMTelemetry to link the recorded event to the other events from
            // the same install flow).
            install,
            permissions: difference,
            resolve,
            reject,
          },
        },
      };
      Services.obs.notifyObservers(subject, "webextension-permission-prompt");
    });
  };
}

function openOptionsInTab(optionsURL) {
  let mainWindow = window.windowRoot.ownerGlobal;
  if ("switchToTabHavingURI" in mainWindow) {
    mainWindow.switchToTabHavingURI(optionsURL, true, {
      relatedToCurrent: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    return true;
  }
  return false;
}

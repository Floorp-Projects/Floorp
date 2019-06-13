/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

"use strict";

/* exported attachUpdateHandler, gBrowser, getBrowserElement, isCorrectlySigned,
 *          isDisabledUnsigned, loadReleaseNotes, openOptionsInTab,
 *          promiseEvent, shouldShowPermissionsPrompt, showPermissionsPrompt */

const {AddonSettings} =
  ChromeUtils.import("resource://gre/modules/addons/AddonSettings.jsm");
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

function promiseEvent(event, target, capture = false) {
  return new Promise(resolve => {
    target.addEventListener(event, resolve, {capture, once: true});
  });
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

async function loadReleaseNotes(uri) {
  const res = await fetch(uri.spec, {credentials: "omit"});

  if (!res.ok) {
    throw new Error("Error loading release notes");
  }

  // Load the content.
  const text = await res.text();

  // Setup the content sanitizer.
  const ParserUtils = Cc["@mozilla.org/parserutils;1"]
    .getService(Ci.nsIParserUtils);
  const flags =
    ParserUtils.SanitizerDropMedia |
    ParserUtils.SanitizerDropNonCSSPresentation |
    ParserUtils.SanitizerDropForms;

  // Sanitize and parse the content to a fragment.
  const context = document.createElement("div");
  return ParserUtils.parseFragment(text, flags, false, uri, context);
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

function shouldShowPermissionsPrompt(addon) {
  if (!WEBEXT_PERMISSION_PROMPTS || !addon.isWebExtension || addon.seen) {
    return false;
  }

  const {origins, permissions} = addon.userPermissions;
  return origins.length > 0 || permissions.length > 0;
}

function showPermissionsPrompt(addon) {
  return new Promise((resolve) => {
    const permissions = addon.userPermissions;
    const target = getBrowserElement();

    const onAddonEnabled = () => {
      // The user has just enabled a sideloaded extension, if the permission
      // can be changed for the extension, show the post-install panel to
      // give the user that opportunity.
      if (addon.permissions &
        AddonManager.PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS) {
        Services.obs.notifyObservers({addon, target},
          "webextension-install-notify");
      }
      resolve();
    };

    const subject = {
      wrappedJSObject: {
        target,
        info: {
          type: "sideload",
          addon,
          icon: addon.iconURL,
          permissions,
          resolve() {
            addon.markAsSeen();
            addon.enable().then(onAddonEnabled);
          },
          reject() {
            // Ignore a cancelled permission prompt.
          },
        },
      },
    };
    Services.obs.notifyObservers(subject, "webextension-permission-prompt");
  });
}

// Stub tabbrowser implementation for use by the tab-modal alert code
// when an alert/prompt/confirm method is called in a WebExtensions options_ui
// page (See Bug 1385548 for rationale).
var gBrowser = {
  getTabModalPromptBox(browser) {
    const parentWindow = window.docShell.chromeEventHandler.ownerGlobal;

    if (parentWindow.gBrowser) {
      return parentWindow.gBrowser.getTabModalPromptBox(browser);
    }

    return null;
  },
};

function isCorrectlySigned(addon) {
  // Add-ons without an "isCorrectlySigned" property are correctly signed as
  // they aren't the correct type for signing.
  return addon.isCorrectlySigned !== false;
}

function isDisabledUnsigned(addon) {
  let signingRequired = (addon.type == "locale") ?
                        AddonSettings.LANGPACKS_REQUIRE_SIGNING :
                        AddonSettings.REQUIRE_SIGNING;
  return signingRequired && !isCorrectlySigned(addon);
}

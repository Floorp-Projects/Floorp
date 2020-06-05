/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  mobileWindowTracker: "resource://gre/modules/GeckoViewWebExtension.jsm",
});

// This function is pretty tightly tied to Extension.jsm.
// Its job is to fill in the |tab| property of the sender.
const getSender = (extension, target, sender) => {
  let tabId = -1;
  if ("tabId" in sender) {
    // The message came from a privileged extension page running in a tab. In
    // that case, it should include a tabId property (which is filled in by the
    // page-open listener below).
    tabId = sender.tabId;
    delete sender.tabId;
  } else if (ChromeUtils.getClassName(target) == "XULFrameElement") {
    tabId = tabTracker.getBrowserData(target).tabId;
  }

  if (tabId != null && tabId >= 0) {
    const tab = extension.tabManager.get(tabId, null);
    if (tab) {
      sender.tab = tab.convert();
    }
  }
};

// Used by Extension.jsm
global.tabGetSender = getSender;

/* eslint-disable mozilla/balanced-listeners */
extensions.on("page-shutdown", (type, context) => {
  if (context.viewType == "tab") {
    const window = context.xulBrowser.ownerGlobal;
    GeckoViewTabBridge.closeTab({
      window,
      extensionId: context.extension.id,
    });
  }
});
/* eslint-enable mozilla/balanced-listeners */

global.openOptionsPage = async extension => {
  const { options_ui } = extension.manifest;
  const extensionId = extension.id;

  if (options_ui.open_in_tab) {
    // Delegate new tab creation and open the options page in the new tab.
    const tab = await GeckoViewTabBridge.createNewTab({
      extensionId,
      createProperties: {
        url: options_ui.page,
        active: true,
      },
    });

    const { browser } = tab;
    const flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

    browser.loadURI(options_ui.page, {
      flags,
      triggeringPrincipal: extension.principal,
    });

    const newWindow = browser.ownerGlobal;
    mobileWindowTracker.setTabActive(newWindow, true);
    return;
  }

  // Delegate option page handling to the app.
  return GeckoViewTabBridge.openOptionsPage(extensionId);
};

extensions.registerModules({
  browserAction: {
    url: "chrome://geckoview/content/ext-browserAction.js",
    schema: "chrome://extensions/content/schemas/browser_action.json",
    scopes: ["addon_parent"],
    manifest: ["browser_action"],
    paths: [["browserAction"]],
  },
  pageAction: {
    url: "chrome://geckoview/content/ext-pageAction.js",
    schema: "chrome://extensions/content/schemas/page_action.json",
    scopes: ["addon_parent"],
    manifest: ["page_action"],
    paths: [["pageAction"]],
  },
  tabs: {
    url: "chrome://geckoview/content/ext-tabs.js",
    schema: "chrome://geckoview/content/schemas/tabs.json",
    scopes: ["addon_parent"],
    paths: [["tabs"]],
  },
  geckoViewAddons: {
    schema: "chrome://geckoview/content/schemas/gecko_view_addons.json",
  },
});

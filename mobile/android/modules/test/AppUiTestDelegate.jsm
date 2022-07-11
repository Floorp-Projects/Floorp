/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AppUiTestDelegate"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  mobileWindowTracker: "resource://gre/modules/GeckoViewWebExtension.jsm",
  GeckoViewTabBridge: "resource://gre/modules/GeckoViewTab.jsm",
});

const TEST_SUPPORT_EXTENSION_ID = "test-runner-support@tests.mozilla.org";

class Delegate {
  _sendMessageToApp(data) {
    const message = {
      type: "GeckoView:WebExtension:Message",
      sender: {
        envType: "addon_child",
        url: "test-runner-support:///",
      },
      data,
      extensionId: TEST_SUPPORT_EXTENSION_ID,
      nativeApp: "test-runner-support",
    };

    return lazy.EventDispatcher.instance.sendRequestForResult(message);
  }

  clickPageAction(window, extensionId) {
    return this._sendMessageToApp({ type: "clickPageAction", extensionId });
  }

  clickBrowserAction(window, extensionId) {
    return this._sendMessageToApp({ type: "clickBrowserAction", extensionId });
  }

  closePageAction(window, extensionId) {
    return this._sendMessageToApp({ type: "closePageAction", extensionId });
  }

  closeBrowserAction(window, extensionId) {
    return this._sendMessageToApp({ type: "closeBrowserAction", extensionId });
  }

  awaitExtensionPanel(window, extensionId) {
    return this._sendMessageToApp({ type: "awaitExtensionPanel", extensionId });
  }

  async removeTab(tab) {
    const window = tab.browser.ownerGlobal;
    await lazy.GeckoViewTabBridge.closeTab({
      window,
      extensionId: TEST_SUPPORT_EXTENSION_ID,
    });
  }

  async openNewForegroundTab(window, url, waitForLoad = true) {
    const tab = await lazy.GeckoViewTabBridge.createNewTab({
      extensionId: TEST_SUPPORT_EXTENSION_ID,
      createProperties: {
        url,
        active: true,
      },
    });

    const { browser } = tab;
    const triggeringPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      Services.io.newURI(url),
      {}
    );

    browser.loadURI(url, {
      flags: Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
      triggeringPrincipal,
    });

    const newWindow = browser.ownerGlobal;
    lazy.mobileWindowTracker.setTabActive(newWindow, true);

    if (!waitForLoad) {
      return tab;
    }

    return new Promise(resolve => {
      const listener = ev => {
        const { browsingContext, internalURL } = ev.detail;

        // Sometimes we arrive here without an internalURL. If that's the
        // case, just keep waiting until we get one.
        if (!internalURL || internalURL == "about:blank") {
          return;
        }

        // Ignore subframes
        if (browsingContext !== browsingContext.top) {
          return;
        }

        resolve(tab);
        browser.removeEventListener("AppTestDelegate:load", listener, true);
      };
      browser.addEventListener("AppTestDelegate:load", listener, true);
    });
  }
}

var AppUiTestDelegate = new Delegate();

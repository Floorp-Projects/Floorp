/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AppTestDelegateParent"];

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

// Each app needs to implement this
XPCOMUtils.defineLazyModuleGetters(this, {
  AppUiTestDelegate: "resource://testing-common/AppUiTestDelegate.jsm",
});

const UUIDGen = Cc["@mozilla.org/uuid-generator;1"].getService(
  Ci.nsIUUIDGenerator
);

class AppTestDelegateParent extends JSWindowActorParent {
  constructor() {
    super();
    this._tabs = new Map();
  }

  get browser() {
    return this.browsingContext.top.embedderElement;
  }

  get window() {
    return this.browser.ownerGlobal;
  }

  async receiveMessage(message) {
    const { extensionId, url, waitForLoad, tabId } = message.data;
    switch (message.name) {
      case "DOMContentLoaded":
      case "load": {
        return this.browser?.dispatchEvent(
          new CustomEvent(`AppTestDelegate:${message.name}`, {
            detail: {
              browsingContext: this.browsingContext,
              ...message.data,
            },
          })
        );
      }
      case "clickPageAction":
        return AppUiTestDelegate.clickPageAction(this.window, extensionId);
      case "clickBrowserAction":
        return AppUiTestDelegate.clickBrowserAction(this.window, extensionId);
      case "closePageAction":
        return AppUiTestDelegate.closePageAction(this.window, extensionId);
      case "closeBrowserAction":
        return AppUiTestDelegate.closeBrowserAction(this.window, extensionId);
      case "awaitExtensionPanel":
        // The desktop delegate returns a <browser>, but that cannot be sent
        // over IPC, so just ignore it. The promise resolves when the panel and
        // its content is fully loaded.
        await AppUiTestDelegate.awaitExtensionPanel(this.window, extensionId);
        return null;
      case "openNewForegroundTab": {
        // We cannot send the tab object across process so let's store it with
        // a unique ID here.
        const uuid = UUIDGen.generateUUID().toString();
        const tab = await AppUiTestDelegate.openNewForegroundTab(
          this.window,
          url,
          waitForLoad
        );
        this._tabs.set(uuid, tab);
        return uuid;
      }
      case "removeTab": {
        const tab = this._tabs.get(tabId);
        this._tabs.delete(tabId);
        return AppUiTestDelegate.removeTab(tab);
      }

      default:
        throw new Error(`Unknown Test API: ${message.name}.`);
    }
  }
}

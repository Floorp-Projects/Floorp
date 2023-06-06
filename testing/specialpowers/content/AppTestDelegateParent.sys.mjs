/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module provides the bridge between the "AppTestDelegate" helper in
 * mochitests and the supporting implementations in AppUiTestDelegate.sys.mjs.
 *
 * "AppTestDelegate" is documented in AppTestDelegate.sys.mjs and enables
 * mochitests to invoke common functionality whose implementation is different
 * (e.g. in browser/ and mobile/ instead of toolkit/).
 * Tests can use this common interface after importing AppTestDelegate.sys.mjs:
 *
 *     // head.js, in the scope of a plain mochitest:
 *     var { AppTestDelegate } = SpecialPowers.ChromeUtils.importESModule(
 *       "resource://specialpowers/AppTestDelegate.sys.mjs"
 *     );
 *
 *     // test usage example: open and close a tab.
 *     let tab = await AppTestDelegate.openNewForegroundTab(window, url);
 *     await AppTestDelegate.removeTab(window, tab);
 *
 * ## Overview of files supporting "AppTestDelegate":
 *
 * MOZ_BUILD_APP-specific AppUiTestDelegate.sys.mjs implementations:
 * - browser/components/extensions/test/AppUiTestDelegate.jsm
 * - mobile/android/modules/test/AppUiTestDelegate.jsm
 * - mail/components/extensions/test/AppUiTestDelegate.sys.mjs (in comm-central)
 *
 * Glue between AppUiTestDelegate.sys.mjs in parent and test code in child:
 * - testing/specialpowers/content/AppTestDelegateParent.sys.mjs (this file)
 * - testing/specialpowers/content/AppTestDelegateChild.sys.mjs
 * - testing/specialpowers/content/AppTestDelegate.sys.mjs
 *
 * Setup for usage by test code in child (i.e. plain mochitests):
 * - Import AppTestDelegate.sys.mjs (e.g. in head.js or the test)
 *
 * Note: some browser-chrome tests import AppUiTestDelegate.sys.mjs directly,
 * but that is not part of this API contract. They merely reuse code.
 *
 * ## How to add new AppTestDelegate methods
 *
 * - Add the method to AppTestDelegate.sys.mjs
 * - Add a message forwarder in AppTestDelegateChild.sys.mjs
 * - Add a message handler in AppTestDelegateParent.sys.mjs
 * - Add an implementation in AppUiTestDelegate.sys.mjs for each MOZ_BUILD_APP,
 *   by defining the method on the exported AppUiTestDelegate object.
 *   All AppUiTestDelegate implementations must be kept in sync to have the
 *   same interface!
 *
 * You should use the same method name across all of these files for ease of
 * lookup and maintainability.
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  // Each app needs to implement this - see above comment.
  AppUiTestDelegate: "resource://testing-common/AppUiTestDelegate.sys.mjs",
});

export class AppTestDelegateParent extends JSWindowActorParent {
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
        return lazy.AppUiTestDelegate.clickPageAction(this.window, extensionId);
      case "clickBrowserAction":
        return lazy.AppUiTestDelegate.clickBrowserAction(
          this.window,
          extensionId
        );
      case "closePageAction":
        return lazy.AppUiTestDelegate.closePageAction(this.window, extensionId);
      case "closeBrowserAction":
        return lazy.AppUiTestDelegate.closeBrowserAction(
          this.window,
          extensionId
        );
      case "awaitExtensionPanel":
        // The desktop delegate returns a <browser>, but that cannot be sent
        // over IPC, so just ignore it. The promise resolves when the panel and
        // its content is fully loaded.
        await lazy.AppUiTestDelegate.awaitExtensionPanel(
          this.window,
          extensionId
        );
        return null;
      case "openNewForegroundTab": {
        // We cannot send the tab object across process so let's store it with
        // a unique ID here.
        const uuid = Services.uuid.generateUUID().toString();
        const tab = await lazy.AppUiTestDelegate.openNewForegroundTab(
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
        return lazy.AppUiTestDelegate.removeTab(tab);
      }

      default:
        throw new Error(`Unknown Test API: ${message.name}.`);
    }
  }
}

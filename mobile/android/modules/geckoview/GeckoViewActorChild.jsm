/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

var EXPORTED_SYMBOLS = ["GeckoViewActorChild"];

class GeckoViewActorChild extends JSWindowActorChild {
  static initLogging(aModuleName) {
    const tag = aModuleName.replace("GeckoView", "") + "[C]";
    return GeckoViewUtils.initLogging(tag);
  }

  actorCreated() {
    this.eventDispatcher = GeckoViewUtils.getDispatcherForWindow(
      this.docShell.domWindow
    );
  }

  /** Returns true if this docShell is of type Content, false otherwise. */
  get isContentWindow() {
    if (!this.docShell) {
      return false;
    }

    // Some WebExtension windows are tagged as content but really they are not
    // for us (e.g. the remote debugging window or the background extension
    // page).
    const browser = this.browsingContext.top.embedderElement;
    if (browser) {
      const viewType = browser.getAttribute("webextension-view-type");
      if (viewType) {
        debug`webextension-view-type: ${viewType}`;
        return false;
      }
      const debugTarget = browser.getAttribute(
        "webextension-addon-debug-target"
      );
      if (debugTarget) {
        debug`webextension-addon-debug-target: ${debugTarget}`;
        return false;
      }
    }

    return this.docShell.itemType == this.docShell.typeContent;
  }
}

const { debug, warn } = GeckoViewUtils.initLogging("Actor[C]");

/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["AutoScrollParent"];

class AutoScrollParent extends JSWindowActorParent {
  receiveMessage(msg) {
    let browser = this.manager.browsingContext.top.embedderElement;
    if (!browser) {
      return null;
    }

    // If another tab is activated, we shouldn't start autoscroll requested
    // for the previous active window if and only if the browser is a remote
    // browser.  This is required for web apps which don't prevent default of
    // middle click after opening a new window.  If the active tab is our
    // documents like about:*, we don't need this check since our documents
    // should do it correctly.
    const requestedInForegroundTab = browser.isRemoteBrowser
      ? Services.focus.focusedElement == browser
      : true;

    let data = msg.data;
    switch (msg.name) {
      case "Autoscroll:Start":
        // Don't start autoscroll if the tab has already been a background tab.
        if (!requestedInForegroundTab) {
          return Promise.resolve({ autoscrollEnabled: false, usingAPZ: false });
        }
        return Promise.resolve(browser.startScroll(data));
      case "Autoscroll:MaybeStartInParent":
        // Don't start autoscroll if the tab has already been a background tab.
        if (!requestedInForegroundTab) {
          return Promise.resolve({ autoscrollEnabled: false, usingAPZ: false });
        }
        let parent = this.browsingContext.parent;
        if (parent) {
          let actor = parent.currentWindowGlobal.getActor("AutoScroll");
          actor.sendAsyncMessage("Autoscroll:MaybeStart", data);
        }
        break;
      case "Autoscroll:Cancel":
        browser.cancelScroll();
        break;
    }
    return null;
  }
}

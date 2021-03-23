/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["AutoScrollParent"];

class AutoScrollParent extends JSWindowActorParent {
  receiveMessage(msg) {
    let browser = this.manager.browsingContext.top.embedderElement;
    if (!browser) {
      return null;
    }

    // If another tab is activated, we shouldn't start autoscroll requested
    // for the previous active window.
    // XXX browsingContext.isActive is not available here because it returns
    //     true when running
    //     browser_cancel_starting_autoscrolling_requested_by_background_tab.js.
    //     Therefore, this checks whether the browser element is focused.
    //     If it's clicked and no new tab is opened foreground, it should have
    //     focus as a default action of the mousedown event.
    const requestedInForegroundTab = Services.focus.focusedElement == browser;

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

/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["AutoplayParent"];

class AutoplayParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    let topBrowsingContext = this.manager.browsingContext.top;
    let browser = topBrowsingContext.embedderElement;
    let document = browser.ownerDocument;
    let event = document.createEvent("CustomEvent");
    event.initCustomEvent("GloballyAutoplayBlocked", true, false, {
      url: this.documentURI,
    });
    browser.dispatchEvent(event);
  }
}

/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ZoomParent"];

class ZoomParent extends JSWindowActorParent {
  receiveMessage(message) {
    let browser = this.browsingContext.top.embedderElement;
    if (!browser) {
      return;
    }

    let document = browser.ownerGlobal.document;

    switch (message.name) {
      case "FullZoomChange": {
        browser._fullZoom = message.data.value;
        let event = document.createEvent("Events");
        event.initEvent("FullZoomChange", true, false);
        browser.dispatchEvent(event);
        break;
      }

      case "TextZoomChange": {
        browser._textZoom = message.data.value;
        let event = document.createEvent("Events");
        event.initEvent("TextZoomChange", true, false);
        browser.dispatchEvent(event);
        break;
      }

      case "ZoomChangeUsingMouseWheel": {
        let event = document.createEvent("Events");
        event.initEvent("ZoomChangeUsingMouseWheel", true, false);
        browser.dispatchEvent(event);
        break;
      }
    }
  }
}

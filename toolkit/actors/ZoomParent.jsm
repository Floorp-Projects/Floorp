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

    /**
     * We respond to requests from the ZoomChild that represent action requests
     * from the platform code. We send matching events on to the frontend
     * FullZoom actor that will take the requested action.
     */

    switch (message.name) {
      case "DoZoomEnlargeBy10": {
        let event = document.createEvent("Events");
        event.initEvent("DoZoomEnlargeBy10", true, false);
        browser.dispatchEvent(event);
        break;
      }

      case "DoZoomReduceBy10": {
        let event = document.createEvent("Events");
        event.initEvent("DoZoomReduceBy10", true, false);
        browser.dispatchEvent(event);
        break;
      }
    }
  }
}

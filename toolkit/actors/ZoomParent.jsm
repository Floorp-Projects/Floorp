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
     * We respond to two types of messages:
     * 1) "Do" messages. These are requests from the ZoomChild that represent
     *    action requests from the platform code. We pass these events on to
     *    the frontend FullZoom actor that will take the requested action.
     * 2) ZoomChange messages. These are updates from the ZoomChild that
     *    changes have already been made to the zoom. We pass these events on
     *    other listeners so that they can also update state. FullZoom has
     *    a PostFullZoomChange method to support automated testing, which
     *    needs to know that both the zoom has changed and the resolution has
     *    been updated by the ZoomChild. TextZoom has no such requirement, and
     *    therefore no need for a PostTextZoomChange event.
     **/

    switch (message.name) {
      case "FullZoomChange": {
        browser._fullZoom = message.data.value;
        let event = document.createEvent("Events");
        event.initEvent("FullZoomChange", true, false);
        browser.dispatchEvent(event);
        break;
      }

      case "PostFullZoomChange": {
        let event = document.createEvent("Events");
        event.initEvent("PostFullZoomChange", true, false);
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

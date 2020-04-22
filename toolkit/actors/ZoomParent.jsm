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
     * We respond to three types of messages:
     * 1) "Do" messages. These are requests from the ZoomChild that represent
     *    action requests from the platform code. We send matching events on
     *    to the frontend FullZoom actor that will take the requested action.
     * 2) ZoomChange messages. These are messages from the ZoomChild that
     *    changes have been made to the zoom by the platform code. We create
     *    events for other listeners so that they can also update state.
     *    These messages will not be sent by the ZoomChild if the zoom change
     *    originated in the ZoomParent actor.
     * 3) FullZoomResolutionStable. This is received after zoom is applied to
     *    a Responsive Design Mode frame and it has reached a stable
     *    resolution. We fire an event that is used by tests.
     **/

    switch (message.name) {
      case "FullZoomChange": {
        let event = document.createEvent("Events");
        event.initEvent("FullZoomChange", true, false);
        browser.dispatchEvent(event);
        break;
      }

      case "FullZoomResolutionStable": {
        let event = document.createEvent("Events");
        event.initEvent("FullZoomResolutionStable", true, false);
        browser.dispatchEvent(event);
        break;
      }

      case "TextZoomChange": {
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

/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BrowserTestUtilsParent"];

class BrowserTestUtilsParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "DOMContentLoaded":
      case "load": {
        // Don't dispatch events that came from stale actors.
        let bc = this.browsingContext;
        if (
          (bc.embedderElement && bc.embedderElement.browsingContext != bc) ||
          !(this.manager && this.manager.isCurrentGlobal)
        ) {
          return;
        }

        let event = new CustomEvent(
          `BrowserTestUtils:ContentEvent:${aMessage.name}`,
          {
            detail: {
              browsingContext: this.browsingContext,
              ...aMessage.data,
            },
          }
        );

        let browser = this.browsingContext.top.embedderElement;
        if (browser) {
          browser.dispatchEvent(event);
        }

        break;
      }
    }
  }
}

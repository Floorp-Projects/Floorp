/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["FindBarParent"];

// Map of browser elements to findbars.
let findbars = new WeakMap();

class FindBarParent extends JSWindowActorParent {
  setFindbar(browser, findbar) {
    if (findbar) {
      findbars.set(browser, findbar);
    } else {
      findbars.delete(browser, findbar);
    }
  }

  receiveMessage(message) {
    let browser = this.manager.browsingContext.top.embedderElement;
    if (!browser) {
      return;
    }

    let respondToMessage = () => {
      let findBar = findbars.get(browser);
      if (!findBar) {
        return;
      }

      switch (message.name) {
        case "Findbar:Keypress":
          findBar._onBrowserKeypress(message.data);
          break;
        case "Findbar:Mouseup":
          findBar.onMouseUp();
          break;
      }
    };

    let findPromise = browser.ownerGlobal.gFindBarPromise;
    if (findPromise) {
      findPromise.then(respondToMessage);
    } else {
      respondToMessage();
    }
  }
}

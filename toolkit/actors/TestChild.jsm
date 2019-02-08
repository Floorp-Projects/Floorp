/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["TestChild"];

class TestChild extends JSWindowActorChild {
  constructor() {
     super();
  }

  recvAsyncMessage(aMessage) {
    switch (aMessage.name) {
      case "toChild":
        aMessage.data.toChild = true;
        this.sendAsyncMessage("Test", "toParent", aMessage.data);
        break;
      case "done":
        this.done(aMessage.data);
        break;
    }
  }

  show() {
    return "TestChild";
  }
}

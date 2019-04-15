/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["TestParent"];

class TestParent extends JSWindowActorParent {
  constructor() {
    super();
    this.wrappedJSObject = this;
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "init":
        aMessage.data.initial =  true;
        this.sendAsyncMessage("toChild", aMessage.data);
        break;
      case "toParent":
        aMessage.data.toParent = true;
        this.sendAsyncMessage("done", aMessage.data);
        break;

      case "event":
        Services.obs.notifyObservers(
          this, "test-js-window-actor-parent-event", aMessage.data.type);
        break;
    }
  }

  show() {
    return "TestParent";
  }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DOMChild"];

const {RemoteAgentActorChild} = ChromeUtils.import("chrome://remote/content/Actor.jsm");

class DOMChild extends RemoteAgentActorChild {
  handleEvent({type}) {
    const event = {
      type,
      timestamp: Date.now(),
    };
    this.sendAsyncMessage("RemoteAgent:DOM:OnEvent", event);
  }
};

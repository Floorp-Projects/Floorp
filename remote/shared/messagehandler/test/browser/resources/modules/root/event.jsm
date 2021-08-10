/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["event"];

class Event {
  constructor(messageHandler) {
    this.messageHandler = messageHandler;
  }

  destroy() {}

  /**
   * Commands
   */

  testEmitRootEvent() {
    this.messageHandler.emitMessageHandlerEvent("event.testRootEvent", {
      text: "event from root",
    });
  }
}

const event = Event;

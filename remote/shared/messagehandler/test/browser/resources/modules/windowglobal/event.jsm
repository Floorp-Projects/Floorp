/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["event"];

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

class Event extends Module {
  destroy() {}

  /**
   * Commands
   */

  testEmitEvent() {
    // Emit a payload including the contextId to check which context emitted
    // a specific event.
    const text = `event from ${this.messageHandler.contextId}`;
    this.messageHandler.emitMessageHandlerEvent("event.testEvent", { text });
  }
}

const event = Event;

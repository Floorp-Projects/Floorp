/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["event"];

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

class EventModule extends Module {
  destroy() {}

  /**
   * Commands
   */

  testEmitInternalEvent() {
    // Emit a payload including the contextId to check which context emitted
    // a specific event.
    const text = `internal event from ${this.messageHandler.contextId}`;
    this.emitEvent("internal-event-from-window-global", { text });
  }

  testEmitProtocolEvent() {
    // Emit a payload including the contextId to check which context emitted
    // a specific protocol event.
    const text = `protocol event from ${this.messageHandler.contextId}`;
    this.emitProtocolEvent("event.testEvent", { text });
  }

  testEmitProtocolEventWithInterception() {
    this.emitProtocolEvent("event.testEventWithInterception", {});
  }
}

const event = EventModule;

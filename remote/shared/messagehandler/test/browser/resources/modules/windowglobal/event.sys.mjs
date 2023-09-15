/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class EventModule extends Module {
  destroy() {}

  /**
   * Commands
   */

  testEmitEvent() {
    // Emit a payload including the contextId to check which context emitted
    // a specific event.
    const text = `event from ${this.messageHandler.contextId}`;
    this.emitEvent("event-from-window-global", { text });
  }

  testEmitEventCancelableWithInterception(params) {
    this.emitEvent("event.testEventCancelableWithInterception", {
      shouldCancel: params.shouldCancel,
    });
  }

  testEmitEventWithInterception() {
    this.emitEvent("event.testEventWithInterception", {});
  }
}

export const event = EventModule;

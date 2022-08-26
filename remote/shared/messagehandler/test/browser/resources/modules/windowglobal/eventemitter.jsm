/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["eventemitter"];

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

class EventEmitterModule extends Module {
  #isSubscribed;

  constructor(messageHandler) {
    super(messageHandler);
    this.#isSubscribed = false;
  }

  destroy() {}

  /**
   * Commands
   */

  emitTestEvent() {
    if (this.#isSubscribed) {
      const text = `event from ${this.messageHandler.contextId}`;
      this.emitEvent("eventemitter.testEvent", { text });
    }

    // Emit another event consistently for monitoring during the test.
    this.emitEvent("eventemitter.monitoringEvent", {});
  }

  isSubscribed() {
    return this.#isSubscribed;
  }

  _applySessionData(params) {
    const { category, added = [], removed = [] } = params;
    if (category === "event") {
      for (const event of added) {
        this.#subscribeEvent(event);
      }
      for (const event of removed) {
        this.#unsubscribeEvent(event);
      }
    }
  }

  #subscribeEvent(event) {
    if (event === "eventemitter.testEvent") {
      if (this.#isSubscribed) {
        throw new Error("Already subscribed to eventemitter.testEvent");
      }
      this.#isSubscribed = true;
    }
  }

  #unsubscribeEvent(event) {
    if (event === "eventemitter.testEvent") {
      if (!this.#isSubscribed) {
        throw new Error("Not subscribed to eventemitter.testEvent");
      }
      this.#isSubscribed = false;
    }
  }
}

const eventemitter = EventEmitterModule;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["internaleventemitter"];

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

class InternalEventEmitterModule extends Module {
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
      const text = `internal event from ${this.messageHandler.contextId}`;
      this.emitEvent("internaleventemitter.testEvent", { text });
    }

    // Emit another event consistently for monitoring during the test.
    this.emitEvent("internaleventemitter.monitoringEvent", {});
  }

  isSubscribed() {
    return this.#isSubscribed;
  }

  _applySessionData(params) {
    const { category, added = [], removed = [] } = params;
    if (category === "internal-event") {
      for (const event of added) {
        this.#subscribeEvent(event);
      }
      for (const event of removed) {
        this.#unsubscribeEvent(event);
      }
    }
  }

  #subscribeEvent(event) {
    if (event === "internaleventemitter.testEvent") {
      if (this.#isSubscribed) {
        throw new Error("Already subscribed to internaleventemitter.testEvent");
      }
      this.#isSubscribed = true;
    }
  }

  #unsubscribeEvent(event) {
    if (event === "internaleventemitter.testEvent") {
      if (!this.#isSubscribed) {
        throw new Error("Not subscribed to internaleventemitter.testEvent");
      }
      this.#isSubscribed = false;
    }
  }
}

const internaleventemitter = InternalEventEmitterModule;

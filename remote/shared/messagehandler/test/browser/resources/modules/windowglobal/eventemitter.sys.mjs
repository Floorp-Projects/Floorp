/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class EventEmitterModule extends Module {
  #isSubscribed;
  #subscribedEvents;

  constructor(messageHandler) {
    super(messageHandler);
    this.#isSubscribed = false;
    this.#subscribedEvents = new Set();
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
    const { category } = params;
    if (category === "event") {
      const filteredSessionData = params.sessionData.filter(item =>
        this.messageHandler.matchesContext(item.contextDescriptor)
      );
      for (const event of this.#subscribedEvents.values()) {
        const hasSessionItem = filteredSessionData.some(
          item => item.value === event
        );
        // If there are no session items for this context, we should unsubscribe from the event.
        if (!hasSessionItem) {
          this.#unsubscribeEvent(event);
        }
      }

      // Subscribe to all events, which have an item in SessionData
      for (const { value } of filteredSessionData) {
        this.#subscribeEvent(value);
      }
    }
  }

  #subscribeEvent(event) {
    if (event === "eventemitter.testEvent") {
      if (this.#isSubscribed) {
        throw new Error("Already subscribed to eventemitter.testEvent");
      }
      this.#isSubscribed = true;
      this.#subscribedEvents.add(event);
    }
  }

  #unsubscribeEvent(event) {
    if (event === "eventemitter.testEvent") {
      if (!this.#isSubscribed) {
        throw new Error("Not subscribed to eventemitter.testEvent");
      }
      this.#isSubscribed = false;
      this.#subscribedEvents.delete(event);
    }
  }
}

export const eventemitter = EventEmitterModule;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContextDescriptorType } from "chrome://remote/content/shared/messagehandler/MessageHandler.sys.mjs";
import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class RootOnlyModule extends Module {
  #sessionDataReceived;
  #subscribedEvents;

  constructor(messageHandler) {
    super(messageHandler);

    this.#sessionDataReceived = [];
    this.#subscribedEvents = new Set();
  }

  destroy() {}

  /**
   * Commands
   */

  getSessionDataReceived() {
    return this.#sessionDataReceived;
  }

  testCommand(params = {}) {
    return params;
  }

  _applySessionData(params) {
    const added = [];
    const removed = [];

    const filteredSessionData = params.sessionData.filter(item =>
      this.messageHandler.matchesContext(item.contextDescriptor)
    );
    for (const event of this.#subscribedEvents.values()) {
      const hasSessionItem = filteredSessionData.some(
        item => item.value === event
      );
      // If there are no session items for this context, we should unsubscribe from the event.
      if (!hasSessionItem) {
        this.#subscribedEvents.delete(event);
        removed.push(event);
      }
    }

    // Subscribe to all events, which have an item in SessionData
    for (const { value } of filteredSessionData) {
      if (!this.#subscribedEvents.has(value)) {
        this.#subscribedEvents.add(value);
        added.push(value);
      }
    }

    this.#sessionDataReceived.push({
      category: params.category,
      added,
      removed,
      contextDescriptor: {
        type: ContextDescriptorType.All,
      },
    });
  }
}

export const rootOnly = RootOnlyModule;

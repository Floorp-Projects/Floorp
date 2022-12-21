/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class CommandModule extends Module {
  constructor(messageHandler) {
    super(messageHandler);
    this._lastSessionDataUpdate = {};
    this._subscribedEvents = new Set();
    this._testCategorySessionData = [];

    this._createdByMessageHandlerConstructor = this._isCreatedByMessageHandlerConstructor();
  }
  destroy() {}

  /**
   * Commands
   */

  _applySessionData(params) {
    if (params.category === "testCategory") {
      const added = [];
      const removed = [];

      const filteredSessionData = params.sessionData.filter(item =>
        this.messageHandler.matchesContext(item.contextDescriptor)
      );
      for (const event of this._subscribedEvents.values()) {
        const hasSessionItem = filteredSessionData.some(
          item => item.value === event
        );
        // If there are no session items for this context, we should unsubscribe from the event.
        if (!hasSessionItem) {
          this._subscribedEvents.delete(event);
          removed.push(event);
        }
      }

      // Subscribe to all events, which have an item in SessionData
      for (const { value } of filteredSessionData) {
        if (!this._subscribedEvents.has(value)) {
          this._subscribedEvents.add(value);
          added.push(value);
        }
      }

      this._testCategorySessionData = this._testCategorySessionData
        .concat(added)
        .filter(value => !removed.includes(value));

      this._lastSessionDataUpdate = {
        addedData: added.join(", "),
        removedData: removed.join(", "),
        sessionData: this._testCategorySessionData.join(", "),
        contextId: this.messageHandler.contextId,
      };
    }

    if (params.category === "browser_session_data_browser_element") {
      this.emitEvent("received-session-data", {
        contextId: this.messageHandler.contextId,
      });
    }

    return {};
  }

  _getSessionDataUpdate(params) {
    const lastUpdate = this._lastSessionDataUpdate;

    // Each "lastUpdate" should only be returned once, so that the caller can
    // assert when a SessionData update had no impact.
    this._lastSessionDataUpdate = null;

    return lastUpdate;
  }

  testWindowGlobalModule() {
    return "windowglobal-value";
  }

  testSetValue(params) {
    const { value } = params;

    this._testValue = value;
  }

  testGetValue() {
    return this._testValue;
  }

  testForwardToWindowGlobal() {
    return "forward-to-windowglobal-value";
  }

  testIsCreatedByMessageHandlerConstructor() {
    return this._createdByMessageHandlerConstructor;
  }

  _isCreatedByMessageHandlerConstructor() {
    let caller = Components.stack.caller;
    while (caller) {
      if (caller.name === this.messageHandler.constructor.name) {
        return true;
      }
      caller = caller.caller;
    }
    return false;
  }
}

export const command = CommandModule;

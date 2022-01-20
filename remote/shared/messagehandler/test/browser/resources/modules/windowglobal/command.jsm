/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["command"];

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

class Command extends Module {
  constructor(messageHandler) {
    super(messageHandler);
    this._testCategorySessionData = [];

    this._createdByMessageHandlerConstructor = this._isCreatedByMessageHandlerConstructor();
  }
  destroy() {}

  /**
   * Commands
   */

  _applySessionData(params) {
    if (params.category === "testCategory") {
      const added = params.added || [];
      const removed = params.removed || [];

      this._testCategorySessionData = this._testCategorySessionData
        .concat(added)
        .filter(value => !removed.includes(value));

      return {
        addedData: added.join(", "),
        removedData: removed.join(", "),
        sessionData: this._testCategorySessionData.join(", "),
        contextId: this.messageHandler.contextId,
      };
    }

    if (params.category === "browser_session_data_browser_element") {
      BrowsingContext.get(
        this.messageHandler.contextId
      ).window.hasSessionDataFlag = true;
    }

    return {};
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

const command = Command;

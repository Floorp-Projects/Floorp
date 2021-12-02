/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["command"];

const { CONTEXT_DESCRIPTOR_TYPES } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/MessageHandler.jsm"
);

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

class Command extends Module {
  destroy() {}

  /**
   * Commands
   */

  testAddSessionData(params) {
    return this.messageHandler.addSessionData({
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: CONTEXT_DESCRIPTOR_TYPES.ALL,
      },
      values: params.values,
    });
  }

  testRemoveSessionData(params) {
    return this.messageHandler.removeSessionData({
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: CONTEXT_DESCRIPTOR_TYPES.ALL,
      },
      values: params.values,
    });
  }

  testRootModule() {
    return "root-value";
  }
}

const command = Command;

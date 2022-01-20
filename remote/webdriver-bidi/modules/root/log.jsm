/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["log"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  CONTEXT_DESCRIPTOR_TYPES:
    "chrome://remote/content/shared/messagehandler/MessageHandler.jsm",
  Module: "chrome://remote/content/shared/messagehandler/Module.jsm",
});

class Log extends Module {
  destroy() {}

  /**
   * Internal commands
   */

  _subscribeEvent(params) {
    // TODO: Bug 1741861. Move this logic to a shared module or the an abstract
    // class.
    switch (params.event) {
      case "log.entryAdded":
        return this.messageHandler.addSessionData({
          moduleName: "log",
          category: "event",
          contextDescriptor: {
            type: CONTEXT_DESCRIPTOR_TYPES.ALL,
          },
          values: ["log.entryAdded"],
        });
      default:
        throw new Error(`Unsupported event for log module ${params.event}`);
    }
  }

  _unsubscribeEvent(params) {
    switch (params.event) {
      case "log.entryAdded":
        return this.messageHandler.removeSessionData({
          moduleName: "log",
          category: "event",
          contextDescriptor: {
            type: CONTEXT_DESCRIPTOR_TYPES.ALL,
          },
          values: ["log.entryAdded"],
        });
      default:
        throw new Error(`Unsupported event for log module ${params.event}`);
    }
  }
}

const log = Log;

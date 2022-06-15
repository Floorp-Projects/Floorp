/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["log"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ContextDescriptorType:
    "chrome://remote/content/shared/messagehandler/MessageHandler.jsm",
});

class LogModule extends Module {
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
            type: lazy.ContextDescriptorType.All,
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
            type: lazy.ContextDescriptorType.All,
          },
          values: ["log.entryAdded"],
        });
      default:
        throw new Error(`Unsupported event for log module ${params.event}`);
    }
  }

  static get supportedEvents() {
    return ["log.entryAdded"];
  }
}

const log = LogModule;

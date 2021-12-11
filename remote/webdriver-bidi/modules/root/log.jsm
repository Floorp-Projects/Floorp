/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["log"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Module: "chrome://remote/content/shared/messagehandler/Module.jsm",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.jsm",
});

class Log extends Module {
  destroy() {}

  /**
   * Internal commands
   */

  _subscribeEvent(params) {
    switch (params.event) {
      case "log.entryAdded":
        return this.messageHandler.handleCommand({
          moduleName: "log",
          commandName: "_subscribeEvent",
          params: {
            event: "log.entryAdded",
          },
          destination: {
            broadcast: true,
            type: WindowGlobalMessageHandler.type,
          },
        });
      default:
        throw new Error(`Unsupported event for log module ${params.event}`);
    }
  }
}

const log = Log;

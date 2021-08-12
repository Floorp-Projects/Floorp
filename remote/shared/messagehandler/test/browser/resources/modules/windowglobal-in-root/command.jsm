/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["command"];

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

class Command extends Module {
  destroy() {}

  /**
   * Commands
   */

  testInterceptModule() {
    return "intercepted-value";
  }

  async testInterceptAndForwardModule(params, destination) {
    const windowGlobalValue = await this.messageHandler.handleCommand({
      moduleName: "command",
      commandName: "testForwardToWindowGlobal",
      destination,
    });
    return "intercepted-and-forward+" + windowGlobalValue;
  }
}

const command = Command;

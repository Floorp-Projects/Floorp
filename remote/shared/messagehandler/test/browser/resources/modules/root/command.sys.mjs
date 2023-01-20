/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class CommandModule extends Module {
  destroy() {}

  /**
   * Commands
   */

  testRootModule() {
    return "root-value";
  }

  testMissingIntermediaryMethod(params, destination) {
    // Spawn a new internal command, but with a commandName which doesn't match
    // any method.
    return this.messageHandler.handleCommand({
      moduleName: "command",
      commandName: "missingMethod",
      destination,
    });
  }
}

export const command = CommandModule;

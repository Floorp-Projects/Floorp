/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class CommandWindowGlobalOnlyModule extends Module {
  destroy() {}

  /**
   * Commands
   */

  testOnlyInWindowGlobal() {
    return "only-in-windowglobal";
  }

  testBroadcast() {
    return `broadcast-${this.messageHandler.contextId}`;
  }

  testBroadcastWithParameter(params) {
    return `broadcast-${this.messageHandler.contextId}-${params.value}`;
  }

  testError() {
    throw new Error("error-from-module");
  }

  testMissingIntermediaryMethod(params, destination) {
    // Spawn a new internal command, but with a commandName which doesn't match
    // any method.
    return this.messageHandler.handleCommand({
      moduleName: "commandwindowglobalonly",
      commandName: "missingMethod",
      destination,
    });
  }
}

export const commandwindowglobalonly = CommandWindowGlobalOnlyModule;

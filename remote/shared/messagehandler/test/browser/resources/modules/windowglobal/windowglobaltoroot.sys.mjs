/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";
import { RootMessageHandler } from "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs";

class WindowGlobalToRootModule extends Module {
  destroy() {}

  /**
   * Commands
   */

  testHandleCommandToRoot(params, destination) {
    return this.messageHandler.handleCommand({
      moduleName: "windowglobaltoroot",
      commandName: "getValueFromRoot",
      destination: {
        type: RootMessageHandler.type,
      },
    });
  }

  testSendRootCommand(params, destination) {
    return this.messageHandler.sendRootCommand({
      moduleName: "windowglobaltoroot",
      commandName: "getValueFromRoot",
    });
  }
}

export const windowglobaltoroot = WindowGlobalToRootModule;

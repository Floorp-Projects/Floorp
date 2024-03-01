/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";
import { RootMessageHandler } from "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs";

class WindowGlobalToRootModule extends Module {
  constructor(messageHandler) {
    super(messageHandler);
    this.#assertContentProcess();
  }

  destroy() {}

  /**
   * Commands
   */

  testHandleCommandToRoot() {
    return this.messageHandler.handleCommand({
      moduleName: "windowglobaltoroot",
      commandName: "getValueFromRoot",
      destination: {
        type: RootMessageHandler.type,
      },
    });
  }

  testSendRootCommand() {
    return this.messageHandler.sendRootCommand({
      moduleName: "windowglobaltoroot",
      commandName: "getValueFromRoot",
    });
  }

  #assertContentProcess() {
    const isContent =
      Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

    if (!isContent) {
      throw new Error("Can only run in a content process");
    }
  }
}

export const windowglobaltoroot = WindowGlobalToRootModule;

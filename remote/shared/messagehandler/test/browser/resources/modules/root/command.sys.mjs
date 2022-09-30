/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContextDescriptorType } from "chrome://remote/content/shared/messagehandler/MessageHandler.sys.mjs";
import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";
import { WindowGlobalMessageHandler } from "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs";

class CommandModule extends Module {
  destroy() {}

  async #getSessionDataUpdateFromAllContexts() {
    const updates = await this.messageHandler.handleCommand({
      moduleName: "command",
      commandName: "_getSessionDataUpdate",
      destination: {
        contextDescriptor: {
          type: ContextDescriptorType.All,
        },
        type: WindowGlobalMessageHandler.type,
      },
    });

    // Filter out null values, which indicate that no new session data was
    // received by the windowglobal module since the last getSessionDataUpdate
    // command.
    return updates.filter(update => update != null);
  }

  /**
   * Commands
   */

  async testAddSessionData(params) {
    await this.messageHandler.addSessionData({
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.All,
      },
      values: params.values,
    });

    return this.#getSessionDataUpdateFromAllContexts();
  }

  async testRemoveSessionData(params) {
    await this.messageHandler.removeSessionData({
      moduleName: "command",
      category: "testCategory",
      contextDescriptor: {
        type: ContextDescriptorType.All,
      },
      values: params.values,
    });

    return this.#getSessionDataUpdateFromAllContexts();
  }

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

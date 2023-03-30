/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

class InputModule extends Module {
  destroy() {}

  async performActions(options = {}) {
    this.assertExperimentalCommandsEnabled("input.performActions");
    const { actions, context: contextId } = options;

    lazy.assert.string(
      contextId,
      `Expected "context" to be a string, got ${contextId}`
    );

    const context = lazy.TabManager.getBrowsingContextById(contextId);

    await this.messageHandler.forwardCommand({
      moduleName: "input",
      commandName: "performActions",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: {
        actions,
      },
    });

    return {};
  }

  static get supportedEvents() {
    return [];
  }
}

export const input = InputModule;

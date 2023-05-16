/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

class InputModule extends Module {
  destroy() {}

  async performActions(options = {}) {
    const { actions, context: contextId } = options;

    lazy.assert.string(
      contextId,
      `Expected "context" to be a string, got ${contextId}`
    );

    const context = lazy.TabManager.getBrowsingContextById(contextId);
    if (!context) {
      throw new lazy.error.NoSuchFrameError(
        `Browsing context with id ${contextId} not found`
      );
    }

    // Bug 1821460: Fetch top-level browsing context.

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

  /**
   * Reset the input state in the provided browsing context.
   *
   * @param {object=} options
   * @param {string} options.context
   *     Id of the browsing context to reset the input state.
   *
   * @throws {InvalidArgumentError}
   *     If <var>context</var> is not valid type.
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   */
  async releaseActions(options = {}) {
    const { context: contextId } = options;

    lazy.assert.string(
      contextId,
      `Expected "context" to be a string, got ${contextId}`
    );

    const context = lazy.TabManager.getBrowsingContextById(contextId);
    if (!context) {
      throw new lazy.error.NoSuchFrameError(
        `Browsing context with id ${contextId} not found`
      );
    }

    // Bug 1821460: Fetch top-level browsing context.

    await this.messageHandler.forwardCommand({
      moduleName: "input",
      commandName: "releaseActions",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: {},
    });

    return {};
  }

  static get supportedEvents() {
    return [];
  }
}

export const input = InputModule;

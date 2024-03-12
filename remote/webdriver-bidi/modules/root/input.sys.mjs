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

  /**
   * Sets the file property of a given input element with type file to a set of file paths.
   *
   * @param {object=} options
   * @param {string} options.context
   *     Id of the browsing context to set the file property
   *     of a given input element.
   * @param {SharedReference} options.element
   *     A reference to a node, which is used as
   *     a target for setting files.
   * @param {Array<string>} options.files
   *     A list of file paths which should be set.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchElementError}
   *     If the input element cannot be found.
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   * @throws {UnableToSetFileInputError}
   *     If the set of file paths was not set to the input element.
   */
  async setFiles(options = {}) {
    const { context: contextId, element, files } = options;

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

    lazy.assert.array(files, `Expected "files" to be an array, got ${files}`);

    for (const file of files) {
      lazy.assert.string(
        file,
        `Expected an element of "files" to be a string, got ${file}`
      );
    }

    await this.messageHandler.forwardCommand({
      moduleName: "input",
      commandName: "setFiles",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: { element, files },
    });
  }

  static get supportedEvents() {
    return [];
  }
}

export const input = InputModule;

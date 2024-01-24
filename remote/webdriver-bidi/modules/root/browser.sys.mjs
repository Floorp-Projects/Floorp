/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  Marionette: "chrome://remote/content/components/Marionette.sys.mjs",
  UserContextManager:
    "chrome://remote/content/shared/UserContextManager.sys.mjs",
});

/**
 * An object that holds information about a user context.
 *
 * @typedef UserContextInfo
 *
 * @property {string} userContext
 *     The id of the user context.
 */

/**
 * Return value for the getUserContexts command.
 *
 * @typedef GetUserContextsResult
 *
 * @property {Array<UserContextInfo>} userContexts
 *     Array of UserContextInfo for the current user contexts.
 */

class BrowserModule extends Module {
  constructor(messageHandler) {
    super(messageHandler);
  }

  destroy() {}

  /**
   * Commands
   */

  /**
   * Terminate all WebDriver sessions and clean up automation state in the remote browser instance.
   *
   * Session clean up and actual broser closure will happen later in WebDriverBiDiConnection class.
   */
  async close() {
    // TODO Bug 1838269. Enable browser.close command for the case of classic + bidi session, when
    // session ending for this type of session is supported.
    if (lazy.Marionette.running) {
      throw new lazy.error.UnsupportedOperationError(
        "Closing browser with the session which was started with Webdriver classic is not supported," +
          "you can use Webdriver classic session delete command which will also close the browser."
      );
    }
  }

  /**
   * Creates a user context.
   *
   * @returns {UserContextInfo}
   *     UserContextInfo object for the created user context.
   */
  async createUserContext() {
    const userContextId = lazy.UserContextManager.createContext("webdriver");
    return { userContext: userContextId };
  }

  /**
   * Returns the list of available user contexts.
   *
   * @returns {GetUserContextsResult}
   *     Object containing an array of UserContextInfo.
   */
  async getUserContexts() {
    const userContexts = lazy.UserContextManager.getUserContextIds().map(
      userContextId => ({
        userContext: userContextId,
      })
    );

    return { userContexts };
  }

  /**
   * Closes a user context and all browsing contexts in it without running
   * beforeunload handlers.
   *
   * @param {object=} options
   * @param {string} options.userContext
   *     Id of the user context to close.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {NoSuchUserContextError}
   *     Raised if the user context id could not be found.
   */
  async removeUserContext(options = {}) {
    const { userContext: userContextId } = options;

    lazy.assert.string(
      userContextId,
      `Expected "userContext" to be a string, got ${userContextId}`
    );

    if (userContextId === lazy.UserContextManager.DEFAULT_CONTEXT_ID) {
      throw new lazy.error.InvalidArgumentError(
        `Default user context cannot be removed`
      );
    }

    if (!lazy.UserContextManager.hasUserContextId(userContextId)) {
      throw new lazy.error.NoSuchUserContextError(
        `User Context with id ${userContextId} was not found`
      );
    }
    lazy.UserContextManager.removeUserContext(userContextId, {
      closeContextTabs: true,
    });
  }
}

// To export the class as lower-case
export const browser = BrowserModule;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  permissions: "chrome://remote/content/shared/Permissions.sys.mjs",
  UserContextManager:
    "chrome://remote/content/shared/UserContextManager.sys.mjs",
});

class PermissionsModule extends Module {
  constructor(messageHandler) {
    super(messageHandler);
  }

  destroy() {}

  /**
   * An object that holds the information about permission descriptor
   * for Webdriver BiDi permissions.setPermission command.
   *
   * @typedef PermissionDescriptor
   *
   * @property {string} name
   *    The name of the permission.
   */

  /**
   * Set to a given permission descriptor a given state on a provided origin.
   *
   * @param {object=} options
   * @param {PermissionDescriptor} options.descriptor
   *     The descriptor of the permission which will be updated.
   * @param {PermissionState} options.state
   *     The state which will be set to the permission.
   * @param {string} options.origin
   *    The origin which is used as a target for permission update.
   * @param {string=} options.userContext [unsupported]
   *    The id of the user context which should be used as a target
   *    for permission update.
   *
   * @throws {InvalidArgumentError}
   *     Raised if an argument is of an invalid type or value.
   * @throws {UnsupportedOperationError}
   *     Raised when unsupported permissions are set or <var>userContext</var>
   *     argument is used.
   */
  async setPermission(options = {}) {
    const {
      descriptor,
      state,
      origin,
      userContext: userContextId = null,
    } = options;

    lazy.permissions.validateDescriptor(descriptor);

    const permissionName = descriptor.name;

    if (permissionName === "storage-access") {
      // TODO: Bug 1895457. Add support for "storage-access" permission.
      throw new lazy.error.UnsupportedOperationError(
        `"descriptor.name" "${permissionName}" is currently unsupported`
      );
    }

    lazy.permissions.validateState(state);

    lazy.assert.string(
      origin,
      `Expected "origin" to be a string, got ${origin}`
    );
    lazy.assert.that(
      origin => URL.canParse(origin),
      `Expected "origin" to be a valid URL, got ${origin}`
    )(origin);

    let userContext;
    if (userContextId !== null) {
      lazy.assert.string(
        userContextId,
        `Expected "userContext" to be a string, got ${userContextId}`
      );

      if (!lazy.UserContextManager.hasUserContextId(userContextId)) {
        throw new lazy.error.NoSuchUserContextError(
          `User Context with id ${userContextId} was not found`
        );
      }

      userContext = lazy.UserContextManager.getInternalIdById(userContextId);
    }

    const activeWindow = Services.wm.getMostRecentBrowserWindow();
    let typedDescriptor;
    try {
      typedDescriptor = activeWindow.navigator.permissions.parseSetParameters({
        descriptor,
        state,
      });
    } catch (err) {
      throw new lazy.error.InvalidArgumentError(
        `The conversion of "descriptor" was not successful: ${err.message}`
      );
    }

    lazy.permissions.set(typedDescriptor, state, origin, userContext);
  }
}

export const permissions = PermissionsModule;

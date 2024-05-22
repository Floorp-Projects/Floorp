/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
});

/**
 * @typedef {string} PermissionState
 */

/**
 * Enum of possible permission states supported by permission APIs.
 *
 * @readonly
 * @enum {PermissionState}
 */
const PermissionState = {
  denied: "denied",
  granted: "granted",
  prompt: "prompt",
};

/** @namespace */
export const permissions = {};

/**
 * Get a permission type for the "storage-access" permission.
 *
 * @param {nsIURI} uri
 *     The URI to use for building the permission type.
 *
 * @returns {string} permissionType
 *    The permission type for the "storage-access" permission.
 */
permissions.getStorageAccessPermissionsType = function (uri) {
  const thirdPartyPrincipalSite = Services.eTLD.getSite(uri);
  return "3rdPartyFrameStorage^" + thirdPartyPrincipalSite;
};

/**
 * Set a permission given a permission descriptor, a permission state,
 * an origin.
 *
 * @param {PermissionDescriptor} descriptor
 *     The descriptor of the permission which will be updated.
 * @param {string} state
 *     State of the permission. It can be `granted`, `denied` or `prompt`.
 * @param {string} origin
 *     The origin which is used as a target for permission update.
 * @param {string=} userContextId
 *     The internal id of the user context which should be used as a target
 *     for permission update.
 *
 * @throws {UnsupportedOperationError}
 *     If <var>state</var> has unsupported value.
 */
permissions.set = function (descriptor, state, origin, userContextId) {
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(origin);

  if (userContextId) {
    principal = Services.scriptSecurityManager.principalWithOA(principal, {
      userContextId,
    });
  }

  switch (state) {
    case "granted": {
      Services.perms.addFromPrincipal(
        principal,
        descriptor.type,
        Services.perms.ALLOW_ACTION
      );
      return;
    }
    case "denied": {
      Services.perms.addFromPrincipal(
        principal,
        descriptor.type,
        Services.perms.DENY_ACTION
      );
      return;
    }
    case "prompt": {
      Services.perms.removeFromPrincipal(principal, descriptor.type);
      return;
    }
    default:
      throw new lazy.error.UnsupportedOperationError(
        "Unrecognized permission keyword for 'Set Permission' operation"
      );
  }
};

/**
 * Validate the permission descriptor.
 *
 * @param {PermissionDescriptor} descriptor
 *     The descriptor of the permission which will be validated.
 *
 * @throws {InvalidArgumentError}
 *     Raised if an argument is of an invalid type or value.
 * @throws {UnsupportedOperationError}
 *     If a permission with <var>descriptor</var> is not supported.
 */
permissions.validateDescriptor = function (descriptor) {
  lazy.assert.object(
    descriptor,
    `Expected "descriptor" to be an object, got ${descriptor}`
  );
  const permissionName = descriptor.name;
  lazy.assert.string(
    permissionName,
    `Expected "descriptor.name" to be a string, got ${permissionName}`
  );

  // Bug 1609427: PermissionDescriptor for "camera" and "microphone" are not yet implemented.
  if (["camera", "microphone"].includes(permissionName)) {
    throw new lazy.error.UnsupportedOperationError(
      `"descriptor.name" "${permissionName}" is currently unsupported`
    );
  }
};

/**
 * Validate the permission state.
 *
 * @param {PermissionState} state
 *     The state of the permission which will be validated.
 *
 * @throws {InvalidArgumentError}
 *     Raised if an argument is of an invalid type or value.
 */
permissions.validateState = function (state) {
  const permissionStateTypes = Object.keys(PermissionState);
  lazy.assert.that(
    state => permissionStateTypes.includes(state),
    `Expected "state" to be one of ${permissionStateTypes}, got ${state}`
  )(state);
};

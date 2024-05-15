/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
});

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
 * Validate the permission.
 *
 * @param {string} permissionName
 *     The name of the permission which will be validated.
 *
 * @throws {UnsupportedOperationError}
 *     If <var>permissionName</var> is not supported.
 */
permissions.validatePermission = function (permissionName) {
  // Bug 1609427: PermissionDescriptor for "camera" and "microphone" are not yet implemented.
  if (["camera", "microphone"].includes(permissionName)) {
    throw new lazy.error.UnsupportedOperationError(
      `"descriptor.name" "${permissionName}" is currently unsupported`
    );
  }
};

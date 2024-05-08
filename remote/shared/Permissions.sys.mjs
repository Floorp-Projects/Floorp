/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  MarionettePrefs: "chrome://remote/content/marionette/prefs.sys.mjs",
});

/** @namespace */
export const permissions = {};

function mapToInternalPermissionParameters(browsingContext, permissionType) {
  const currentURI = browsingContext.currentWindowGlobal.documentURI;

  // storage-access is quite special...
  if (permissionType === "storage-access") {
    const thirdPartyPrincipalSite = Services.eTLD.getSite(currentURI);

    const topLevelURI = browsingContext.top.currentWindowGlobal.documentURI;
    const topLevelPrincipal =
      Services.scriptSecurityManager.createContentPrincipal(topLevelURI, {});

    return {
      name: "3rdPartyFrameStorage^" + thirdPartyPrincipalSite,
      principal: topLevelPrincipal,
    };
  }

  const currentPrincipal =
    Services.scriptSecurityManager.createContentPrincipal(currentURI, {});

  return {
    name: permissionType,
    principal: currentPrincipal,
  };
}

/**
 * Set a permission's state.
 * Note: Currently just a shim to support testdriver's set_permission.
 *
 * @param {object} permissionType
 *     The Gecko internal permission type
 * @param {string} state
 *     State of the permission. It can be `granted`, `denied` or `prompt`.
 * @param {boolean} oneRealm
 *     Currently ignored
 * @param {browsingContext=} browsingContext
 *     Current browsing context object
 * @throws {UnsupportedOperationError}
 *     If `marionette.setpermission.enabled` is not set or
 *     an unsupported permission is used.
 */
permissions.set = function (permissionType, state, oneRealm, browsingContext) {
  if (!lazy.MarionettePrefs.setPermissionEnabled) {
    throw new lazy.error.UnsupportedOperationError(
      "'Set Permission' is not available"
    );
  }

  const { name, principal } = mapToInternalPermissionParameters(
    browsingContext,
    permissionType
  );

  switch (state) {
    case "granted": {
      Services.perms.addFromPrincipal(
        principal,
        name,
        Services.perms.ALLOW_ACTION
      );
      return;
    }
    case "denied": {
      Services.perms.addFromPrincipal(
        principal,
        name,
        Services.perms.DENY_ACTION
      );
      return;
    }
    case "prompt": {
      Services.perms.removeFromPrincipal(principal, name);
      return;
    }
    default:
      throw new lazy.error.UnsupportedOperationError(
        "Unrecognized permission keyword for 'Set Permission' operation"
      );
  }
};

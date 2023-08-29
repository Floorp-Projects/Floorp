/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  MarionettePrefs: "chrome://remote/content/marionette/prefs.sys.mjs",
});

/** @namespace */
export const permissions = {};

/**
 * Set a permission's state.
 * Note: Currently just a shim to support testdriver's set_permission.
 *
 * @param {object} descriptor
 *     Descriptor with the `name` property.
 * @param {string} state
 *     State of the permission. It can be `granted`, `denied` or `prompt`.
 * @param {boolean} oneRealm
 *     Currently ignored
 * @param {browsingContext=} thirdPartyBrowsingContext
 *     3rd party browsing context object
 * @param {browsingContext=} topLevelBrowsingContext
 *     Top level browsing context object
 * @throws {UnsupportedOperationError}
 *     If `marionette.setpermission.enabled` is not set or
 *     an unsupported permission is used.
 */
permissions.set = function (
  descriptor,
  state,
  oneRealm,
  thirdPartyBrowsingContext,
  topLevelBrowsingContext
) {
  if (!lazy.MarionettePrefs.setPermissionEnabled) {
    throw new lazy.error.UnsupportedOperationError(
      "'Set Permission' is not available"
    );
  }

  // This is not a real implementation of the permissions API.
  // Instead the purpose of this implementation is to have web-platform-tests
  // that use `set_permission()` not fail.
  // Each test needs the corresponding testing pref to make it actually work.
  const { name } = descriptor;

  if (state === "prompt" && name !== "storage-access") {
    throw new lazy.error.UnsupportedOperationError(
      "'Set Permission' doesn't support prompt"
    );
  }
  if (["clipboard-write", "clipboard-read"].includes(name)) {
    if (
      Services.prefs.getBoolPref("dom.events.testing.asyncClipboard", false)
    ) {
      return;
    }
    throw new lazy.error.UnsupportedOperationError(
      "'Set Permission' expected dom.events.testing.asyncClipboard to be set"
    );
  } else if (name === "notifications") {
    if (Services.prefs.getBoolPref("notification.prompt.testing", false)) {
      return;
    }
    throw new lazy.error.UnsupportedOperationError(
      "'Set Permission' expected notification.prompt.testing to be set"
    );
  } else if (name === "storage-access") {
    // Check if browsing context has a window global object
    lazy.assert.open(thirdPartyBrowsingContext);
    lazy.assert.open(topLevelBrowsingContext);

    const thirdPartyURI =
      thirdPartyBrowsingContext.currentWindowGlobal.documentURI;
    const topLevelURI = topLevelBrowsingContext.currentWindowGlobal.documentURI;

    const thirdPartyPrincipalSite = Services.eTLD.getSite(thirdPartyURI);

    const topLevelPrincipal =
      Services.scriptSecurityManager.createContentPrincipal(topLevelURI, {});

    switch (state) {
      case "granted": {
        Services.perms.addFromPrincipal(
          topLevelPrincipal,
          "3rdPartyFrameStorage^" + thirdPartyPrincipalSite,
          Services.perms.ALLOW_ACTION
        );
        return;
      }
      case "denied": {
        Services.perms.addFromPrincipal(
          topLevelPrincipal,
          "3rdPartyFrameStorage^" + thirdPartyPrincipalSite,
          Services.perms.DENY_ACTION
        );
        return;
      }
      case "prompt": {
        Services.perms.removeFromPrincipal(
          topLevelPrincipal,
          "3rdPartyFrameStorage^" + thirdPartyPrincipalSite
        );
        return;
      }
      default:
        throw new lazy.error.UnsupportedOperationError(
          `'Set Permission' did not work for storage-access'`
        );
    }
  }

  throw new lazy.error.UnsupportedOperationError(
    `'Set Permission' doesn't support '${name}'`
  );
};

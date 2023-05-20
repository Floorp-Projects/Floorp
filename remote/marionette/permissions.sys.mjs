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
 *
 * @throws {UnsupportedOperationError}
 *     If `marionette.setpermission.enabled` is not set or
 *     an unsupported permission is used.
 */
permissions.set = function (descriptor, state, oneRealm) {
  if (!lazy.MarionettePrefs.setPermissionEnabled) {
    throw new lazy.error.UnsupportedOperationError(
      "'Set Permission' is not available"
    );
  }

  if (state === "prompt") {
    throw new lazy.error.UnsupportedOperationError(
      "'Set Permission' doesn't support prompt"
    );
  }

  // This is not a real implementation of the permissions API.
  // Instead the purpose of this implementation is to have web-platform-tests
  // that use `set_permission()` not fail.
  // Each test needs the corresponding testing pref to make it actually work.
  const { name } = descriptor;
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
  }

  throw new lazy.error.UnsupportedOperationError(
    `'Set Permission' doesn't support '${name}'`
  );
};

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["permissions"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  MarionettePrefs: "chrome://remote/content/marionette/prefs.js",
});

/** @namespace */
const permissions = {};

/**
 * Set a permission's state.
 * Note: Currently just a shim to support testdriver's set_permission.
 *
 * @param {Object} descriptor
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
permissions.set = function(descriptor, state, oneRealm) {
  if (!lazy.MarionettePrefs.setPermissionEnabled) {
    throw new lazy.error.UnsupportedOperationError(
      "'Set Permission' is not available"
    );
  }

  const { name } = descriptor;
  if (!["clipboard-write", "clipboard-read"].includes(name)) {
    throw new lazy.error.UnsupportedOperationError(
      `'Set Permission' doesn't support '${name}'`
    );
  }

  if (state === "prompt") {
    throw new lazy.error.UnsupportedOperationError(
      "'Set Permission' doesn't support prompt"
    );
  }

  // This is not a real implementation of the permissions API.
  // Instead the purpose of this implementation is to have web-platform-tests
  // that use `set_permission('clipboard-write|read')` not fail.
  // We enable dom.events.testing.asyncClipboard for the whole test suite anyway,
  // so no extra permission is necessary.
  if (!Services.prefs.getBoolPref("dom.events.testing.asyncClipboard", false)) {
    throw new lazy.error.UnsupportedOperationError(
      "'Set Permission' expected dom.events.testing.asyncClipboard to be set"
    );
  }
};

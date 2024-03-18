/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1831007 - Shim window.OnetrustActiveGroups for Nintendo sites
 *
 * Nintendo relies on `window.OnetrustActiveGroups` being defined. If it's not,
 * users may have intermittent issues signing into their account, as they're
 * then trying to call `.split()` on `undefined`.
 *
 * This intervention sets a default value (an empty string), but still allows
 * the value to be overwritten at any time.
 */

/* globals exportFunction */

console.info(
  "The window.OnetrustActiveGroups property has been shimmed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1831007 for details."
);

Object.defineProperty(window.wrappedJSObject, "OnetrustActiveGroups", {
  value: "",
  writable: true,
});

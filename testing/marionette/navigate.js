/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

this.EXPORTED_SYMBOLS = ["navigate"];

/** @namespace */
this.navigate = {};

/**
 * Determines if we expect to get a DOM load event (DOMContentLoaded)
 * on navigating to the <code>future</code> URL.
 *
 * @param {string} current
 *     URL the browser is currently visiting.
 * @param {string=} future
 *     Destination URL, if known.
 *
 * @return {boolean}
 *     Full page load would be expected if future is followed.
 *
 * @throws TypeError
 *     If <code>current</code> is not defined, or any of
 *     <code>current</code> or <code>future</code>  are invalid URLs.
 */
navigate.isLoadEventExpected = function(current, future = undefined) {
  // assume we will go somewhere exciting
  if (typeof current == "undefined") {
    throw new TypeError("Expected at least one URL");
  }

  // Assume we will go somewhere exciting
  if (typeof future == "undefined") {
    return true;
  }

  let cur = new URL(current);
  let fut = new URL(future);

  // Assume javascript:<whatever> will modify the current document
  // but this is not an entirely safe assumption to make,
  // considering it could be used to set window.location
  if (fut.protocol == "javascript:") {
    return false;
  }

  // If hashes are present and identical
  if (cur.href.includes("#") && fut.href.includes("#") &&
      cur.hash === fut.hash) {
    return false;
  }

  return true;
};

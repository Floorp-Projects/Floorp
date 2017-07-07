/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.importGlobalProperties(["URL"]);

this.EXPORTED_SYMBOLS = ["navigate"];

this.navigate = {};

/**
 * Determines if we expect to get a DOM load event (DOMContentLoaded)
 * on navigating to the |future| URL.
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
 *     If |current| is not defined, or any of |current| or |future|
 *     are invalid URLs.
 */
navigate.isLoadEventExpected = function(current, future = undefined) {
  // assume we will go somewhere exciting
  if (typeof current == "undefined") {
    throw TypeError("Expected at least one URL");
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

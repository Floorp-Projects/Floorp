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
 * on navigating to the |url|.
 *
 * @param {string} url
 *     Destination URL
 *
 * @return {boolean}
 *     Full page load would be expected if url gets loaded.
 *
 * @throws TypeError
 *     If |url| is an invalid URL.
 */
navigate.isLoadEventExpected = function(url) {
  // assume we will go somewhere exciting
  if (typeof url == "undefined") {
    throw TypeError("Expected destination URL");
  }

  switch (new URL(url).protocol) {
    // assume javascript:<whatever> will modify current document
    // but this is not an entirely safe assumption to make,
    // considering it could be used to set window.location
    case "javascript:":
      return false;
  }

  return true;
};

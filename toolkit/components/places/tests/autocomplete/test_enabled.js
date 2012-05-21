/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 471903 to make sure searching in autocomplete can be turned on
 * and off. Also test bug 463535 for pref changing search.
 */

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
  "http://url/0",
];
let kTitles = [
  "title",
];

addPageBook(0, 0); // visited page

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
let gTests = [
  ["1: plain search",
   "url", [0]],
  ["2: search disabled",
   "url", [], function() setSearch(0)],
  ["3: resume normal search",
   "url", [0], function() setSearch(1)],
];

function setSearch(aSearch) {
  prefs.setBoolPref("browser.urlbar.autocomplete.enabled", !!aSearch);
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 451760 which allows matching only at the beginning of urls or
 * titles to simulate Firefox 2 functionality.
 */

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
  "http://x.com/y",
  "https://y.com/x",
];
let kTitles = [
  "a b",
  "b a",
];

addPageBook(0, 0);
addPageBook(1, 1);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
let gTests = [
  // Tests after this one will match at the beginning
  ["0: Match at the beginning of titles",
   "a", [0],
   function() setBehavior(3)],
  ["1: Match at the beginning of titles",
   "b", [1]],
  ["2: Match at the beginning of urls",
   "x", [0]],
  ["3: Match at the beginning of urls",
   "y", [1]],
  
  // Tests after this one will match against word boundaries and anywhere
  ["4: Sanity check that matching anywhere finds more",
   "a", [0,1],
   function() setBehavior(1)],
];

function setBehavior(aType) {
  prefs.setIntPref("browser.urlbar.matchBehavior", aType);
}

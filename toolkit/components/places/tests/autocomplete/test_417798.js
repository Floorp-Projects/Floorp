/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 417798 to make sure javascript: URIs don't show up unless the
 * user searches for javascript: explicitly.
 */

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
  "http://abc/def",
  "javascript:5",
];
let kTitles = [
  "Title with javascript:",
];

addPageBook(0, 0); // regular url
// javascript: uri as bookmark (no visit)
addPageBook(1, 0, 0, undefined, undefined, undefined, true);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
let gTests = [
  ["0: Match non-javascript: with plain search",
   "a", [0]],
  ["1: Match non-javascript: with almost javascript:",
   "javascript", [0]],
  ["2: Match javascript:",
   "javascript:", [0,1]],
  ["3: Match nothing with non-first javascript:",
   "5 javascript:", []],
  ["4: Match javascript: with multi-word search",
   "javascript: 5", [1]],
];

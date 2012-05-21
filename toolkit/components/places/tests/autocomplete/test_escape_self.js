/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 422698 to make sure searches with urls from the location bar
 * correctly match itself when it contains escaped characters.
 */

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
  "http://unescapeduri/",
  "http://escapeduri/%40/",
];
let kTitles = [
  "title",
];

// Add unescaped and escaped uris
addPageBook(0, 0);
addPageBook(1, 0);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
let gTests = [
  ["0: Unescaped location matches itself",
   kURIs[0], [0]],
  ["1: Escaped location matches itself",
   kURIs[1], [1]],
];

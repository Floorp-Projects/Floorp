/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 424509 to make sure searching for "h" doesn't match "http" of urls.
 */

// Define some shared uris and titles (each page needs its own uri)
var kURIs = [
  "http://site/",
  "http://happytimes/",
];
var kTitles = [
  "title",
];

// Add site without "h" and with "h"
addPageBook(0, 0);
addPageBook(1, 0);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
var gTests = [
  ["0: Searching for h matches site and not http://",
   "h", [1]],
];

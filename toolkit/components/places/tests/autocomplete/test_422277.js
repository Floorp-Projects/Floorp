/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 422277 to make sure bad escaped uris don't get escaped. This makes
 * sure we don't hit an assertion for "not a UTF8 string".
 */

// Define some shared uris and titles (each page needs its own uri)
var kURIs = [
  "http://site/%EAid",
];
var kTitles = [
  "title",
];

addPageBook(0, 0);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
var gTests = [
  ["0: Bad escaped uri stays escaped",
   "site", [0]],
];

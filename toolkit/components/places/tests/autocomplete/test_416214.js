/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test autocomplete for non-English URLs that match the tag bug 416214. Also
 * test bug 417441 by making sure escaped ascii characters like "+" remain
 * escaped.
 *
 * - add a visit for a page with a non-English URL
 * - add a tag for the page
 * - search for the tag
 * - test number of matches (should be exactly one)
 * - make sure the url is decoded
 */

let theTag = "superTag";

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
  "http://escaped/ユニコード",
  "http://asciiescaped/blocking-firefox3%2B",
];
let kTitles = [
  "title",
  theTag,
];

// Add pages that match the tag
addPageBook(0, 0, 0, [1]);
addPageBook(1, 0, 0, [1]);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
let gTests = [
  ["0: Make sure tag matches return the right url as well as '+' remain escaped",
   theTag, [0,1]],
];

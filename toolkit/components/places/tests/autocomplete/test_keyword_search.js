/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 392143 that puts keyword results into the autocomplete. Makes
 * sure that multiple parameter queries get spaces converted to +, + converted
 * to %2B, non-ascii become escaped, and pages in history that match the
 * keyword uses the page's title.
 *
 * Also test for bug 249468 by making sure multiple keyword bookmarks with the
 * same keyword appear in the list.
 */

// Details for the keyword bookmark
let keyBase = "http://abc/?search=";
let keyKey = "key";

// A second keyword bookmark with the same keyword
let otherBase = "http://xyz/?foo=";

let unescaped = "ユニコード";
let pageInHistory = "ThisPageIsInHistory";

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
  keyBase + "%s",
  keyBase + "term",
  keyBase + "multi+word",
  keyBase + "blocking%2B",
  keyBase + unescaped,
  keyBase + pageInHistory,
  keyBase,
  otherBase + "%s",
  keyBase + "twoKey",
  otherBase + "twoKey"
];
let kTitles = [
  "Generic page title",
  "Keyword title",
];

// Add the keyword bookmark
addPageBook(0, 0, 1, [], keyKey);
// Add in the "fake pages" for keyword searches
gPages[1] = [1,1];
gPages[2] = [2,1];
gPages[3] = [3,1];
gPages[4] = [4,1];
// Add a page into history
addPageBook(5, 0);
gPages[6] = [6,1];

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
let gTests = [
  ["0: Plain keyword query",
   keyKey + " term", [1]],
  ["1: Multi-word keyword query",
   keyKey + " multi word", [2]],
  ["2: Keyword query with +",
   keyKey + " blocking+", [3]],
  ["3: Unescaped term in query",
   keyKey + " " + unescaped, [4]],
  ["4: Keyword that happens to match a page",
   keyKey + " " + pageInHistory, [5]],
  ["5: Keyword without query (without space)",
   keyKey, [6]],
  ["6: Keyword without query (with space)",
   keyKey + " ", [6]],

  // This adds a second keyword so anything after this will match 2 keywords
  ["7: Two keywords matched",
   keyKey + " twoKey", [8,9],
   function() {
     // Add the keyword search as well as search results
     addPageBook(7, 0, 1, [], keyKey);
     gPages[8] = [8,1];
     gPages[9] = [9,1];
   }]
];

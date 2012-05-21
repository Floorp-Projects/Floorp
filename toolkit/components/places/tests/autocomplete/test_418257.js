/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 418257 by making sure tags are returned with the title as part of
 * the "comment" if there are tags even if we didn't match in the tags. They
 * are separated from the title by a endash.
 */

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
  "http://page1",
  "http://page2",
  "http://page3",
  "http://page4",
];
let kTitles = [
  "tag1",
  "tag2",
  "tag3",
];

// Add pages with varying number of tags
addPageBook(0, 0, 0, [0]);
addPageBook(1, 0, 0, [0,1]);
addPageBook(2, 0, 0, [0,2]);
addPageBook(3, 0, 0, [0,1,2]);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
let gTests = [
  ["0: Make sure tags come back in the title when matching tags",
   "page1 tag", [0]],
  ["1: Check tags in title for page2",
   "page2 tag", [1]],
  ["2: Make sure tags appear even when not matching the tag",
   "page3", [2]],
  ["3: Multiple tags come in commas for page4",
   "page4", [3]],
  ["4: Extra test just to make sure we match the title",
   "tag2", [1,3]],
];

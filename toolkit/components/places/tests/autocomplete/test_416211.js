/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test bug 416211 to make sure results that match the tag show the bookmark
 * title instead of the page title.
 */

var theTag = "superTag";

// Define some shared uris and titles (each page needs its own uri)
var kURIs = [
  "http://theuri/",
];
var kTitles = [
  "Page title",
  "Bookmark title",
  theTag,
];

// Add page with a title, bookmark, and [tags]
addPageBook(0, 0, 1, [2]);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
var gTests = [
  ["0: Make sure the tag match gives the bookmark title",
   theTag, [0]],
];

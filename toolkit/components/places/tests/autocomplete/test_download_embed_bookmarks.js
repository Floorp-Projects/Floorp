/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests bug 449406 to ensure that TRANSITION_DOWNLOAD, TRANSITION_EMBED and
 * TRANSITION_FRAMED_LINK bookmarked uri's show up in the location bar.
 */

// Define some shared uris and titles (each page needs its own uri)
var kURIs = [
  "http://download/bookmarked",
  "http://embed/bookmarked",
  "http://framed/bookmarked",
  "http://download",
  "http://embed",
  "http://framed",
];
var kTitles = [
  "download-bookmark",
  "embed-bookmark",
  "framed-bookmark",
  "download2",
  "embed2",
  "framed2",
];

// Add download and embed uris
addPageBook(0, 0, 0, undefined, undefined, TRANSITION_DOWNLOAD);
addPageBook(1, 1, 1, undefined, undefined, TRANSITION_EMBED);
addPageBook(2, 2, 2, undefined, undefined, TRANSITION_FRAMED_LINK);
addPageBook(3, 3, undefined, undefined, undefined, TRANSITION_DOWNLOAD);
addPageBook(4, 4, undefined, undefined, undefined, TRANSITION_EMBED);
addPageBook(5, 5, undefined, undefined, undefined, TRANSITION_FRAMED_LINK);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
var gTests = [
  ["0: Searching for bookmarked download uri matches",
   kTitles[0], [0]],
  ["1: Searching for bookmarked embed uri matches",
   kTitles[1], [1]],
  ["2: Searching for bookmarked framed uri matches",
   kTitles[2], [2]],
  ["3: Searching for download uri does not match",
   kTitles[3], []],
  ["4: Searching for embed uri does not match",
   kTitles[4], []],
  ["5: Searching for framed uri does not match",
   kTitles[5], []],
];

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 401869 to allow multiple words separated by spaces to match in
 * the page title, page url, or bookmark title to be considered a match. All
 * terms must match but not all terms need to be in the title, etc.
 *
 * Test bug 424216 by making sure bookmark titles are always shown if one is
 * available. Also bug 425056 makes sure matches aren't found partially in the
 * page title and partially in the bookmark.
 */

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
  "http://a.b.c/d-e_f/h/t/p",
  "http://d.e.f/g-h_i/h/t/p",
  "http://g.h.i/j-k_l/h/t/p",
  "http://j.k.l/m-n_o/h/t/p",
];
let kTitles = [
  "f(o)o b<a>r",
  "b(a)r b<a>z",
];

// Regular pages
addPageBook(0, 0);
addPageBook(1, 1);
// Bookmarked pages
addPageBook(2, 0, 0);
addPageBook(3, 0, 1);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
let gTests = [
  ["0: Match 2 terms all in url",
   "c d", [0]],
  ["1: Match 1 term in url and 1 term in title",
   "b e", [0,1]],
  ["2: Match 3 terms all in title; display bookmark title if matched",
   "b a z", [1,3]],
  ["3: Match 2 terms in url and 1 in title; make sure bookmark title is used for search",
   "k f t", [2]],
  ["4: Match 3 terms in url and 1 in title",
   "d i g z", [1]],
  ["5: Match nothing",
   "m o z i", []],
];

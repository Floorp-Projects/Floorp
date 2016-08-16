/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 426864 that makes sure the empty search (drop down list) only
 * shows typed pages from history.
 */

// Define some shared uris and titles (each page needs its own uri)
var kURIs = [
  "http://foo/0",
  "http://foo/1",
  "http://foo/2",
  "http://foo/3",
  "http://foo/4",
  "http://foo/5",
];
var kTitles = [
  "title",
];

// Visited (in history)
addPageBook(0, 0); // history
addPageBook(1, 0, 0); // bookmark
addPageBook(2, 0); // history typed
addPageBook(3, 0, 0); // bookmark typed

// Unvisited bookmark
addPageBook(4, 0, 0); // bookmark
addPageBook(5, 0, 0); // bookmark typed

// Set some pages as typed
markTyped([2, 3, 5], 0);
// Remove pages from history to treat them as unvisited
removePages([4, 5]);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
var gTests = [
  ["0: Match everything",
   "foo", [0, 1, 2, 3, 4, 5]],
  ["1: Match only typed history",
   "foo ^ ~", [2, 3]],
  ["2: Drop-down empty search matches only typed history",
   "", [2, 3]],
  ["3: Drop-down empty search matches only bookmarks",
   "", [2, 3], matchBookmarks],
  ["4: Drop-down empty search matches only typed",
   "", [2, 3], matchTyped],
];

function matchBookmarks() {
  prefs.setBoolPref("browser.urlbar.suggest.history", false);
  prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  clearPrefs();
}

function matchTyped() {
  prefs.setBoolPref("browser.urlbar.suggest.history", true);
  prefs.setBoolPref("browser.urlbar.suggest.history.onlyTyped", true);
  clearPrefs();
}

function clearPrefs() {
  prefs.clearUserPref("browser.urlbar.suggest.history");
  prefs.clearUserPref("browser.urlbar.suggest.bookmark");
  prefs.clearUserPref("browser.urlbar.suggest.history.onlyTyped");
}

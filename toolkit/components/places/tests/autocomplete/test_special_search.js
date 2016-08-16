/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 395161 that allows special searches that restrict results to
 * history/bookmark/tagged items and title/url matches.
 *
 * Test 485122 by making sure results don't have tags when restricting result
 * to just history either by default behavior or dynamic query restrict.
 */

// Define some shared uris and titles (each page needs its own uri)
var kURIs = [
  "http://url/",
  "http://url/2",
  "http://foo.bar/",
  "http://foo.bar/2",
  "http://url/star",
  "http://url/star/2",
  "http://foo.bar/star",
  "http://foo.bar/star/2",
  "http://url/tag",
  "http://url/tag/2",
  "http://foo.bar/tag",
  "http://foo.bar/tag/2",
];
var kTitles = [
  "title",
  "foo.bar",
];

// Plain page visits
addPageBook(0, 0); // plain page
addPageBook(1, 1); // title
addPageBook(2, 0); // url
addPageBook(3, 1); // title and url

// Bookmarked pages (no tag)
addPageBook(4, 0, 0); // bookmarked page
addPageBook(5, 1, 1); // title
addPageBook(6, 0, 0); // url
addPageBook(7, 1, 1); // title and url

// Tagged pages
addPageBook(8, 0, 0, [1]); // tagged page
addPageBook(9, 1, 1, [1]); // title
addPageBook(10, 0, 0, [1]); // url
addPageBook(11, 1, 1, [1]); // title and url

// Remove pages from history to treat them as unvisited, so pages that do have
// visits are 0,1,2,3,5,10
removePages([4, 6, 7, 8, 9, 11]);
// Set some pages as typed
markTyped([0, 10], 0);
markTyped([3], 1);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
var gTests = [
  // Test restricting searches
  ["0: History restrict",
   "^", [0, 1, 2, 3, 5, 10]],
  ["1: Star restrict",
   "*", [4, 5, 6, 7, 8, 9, 10, 11]],
  ["2: Tag restrict",
   "+", [8, 9, 10, 11]],

  // Test specials as any word position
  ["3: Special as first word",
   "^ foo bar", [1, 2, 3, 5, 10]],
  ["4: Special as middle word",
   "foo ^ bar", [1, 2, 3, 5, 10]],
  ["5: Special as last word",
   "foo bar ^", [1, 2, 3, 5, 10]],

  // Test restricting and matching searches with a term
  ["6.1: foo ^ -> history",
   "foo ^", [1, 2, 3, 5, 10]],
  ["6.2: foo | -> history (change pref)",
   "foo |", [1, 2, 3, 5, 10], () => changeRestrict("history", "|")],
  ["7.1: foo * -> is star",
   "foo *", [5, 6, 7, 8, 9, 10, 11], () => resetRestrict("history")],
  ["7.2: foo | -> is star (change pref)",
   "foo |", [5, 6, 7, 8, 9, 10, 11], () => changeRestrict("bookmark", "|")],
  ["8.1: foo # -> in title",
   "foo #", [1, 3, 5, 7, 8, 9, 10, 11], () => resetRestrict("bookmark")],
  ["8.2: foo | -> in title (change pref)",
   "foo |", [1, 3, 5, 7, 8, 9, 10, 11], () => changeRestrict("title", "|")],
  ["9.1: foo @ -> in url",
   "foo @", [2, 3, 6, 7, 10, 11], () => resetRestrict("title")],
  ["9.2: foo | -> in url (change pref)",
   "foo |", [2, 3, 6, 7, 10, 11], () => changeRestrict("url", "|")],
  ["10: foo + -> is tag",
   "foo +", [8, 9, 10, 11], () => resetRestrict("url")],
  ["10.2: foo | -> is tag (change pref)",
   "foo |", [8, 9, 10, 11], () => changeRestrict("tag", "|")],
  ["10.3: foo ~ -> is typed",
   "foo ~", [3, 10], () => resetRestrict("tag")],
  ["10.4: foo | -> is typed (change pref)",
   "foo |", [3, 10], () => changeRestrict("typed", "|")],

  // Test various pairs of special searches
  ["11: foo ^ * -> history, is star",
   "foo ^ *", [5, 10], () => resetRestrict("typed")],
  ["12: foo ^ # -> history, in title",
   "foo ^ #", [1, 3, 5, 10]],
  ["13: foo ^ @ -> history, in url",
   "foo ^ @", [2, 3, 10]],
  ["14: foo ^ + -> history, is tag",
   "foo ^ +", [10]],
  ["14.1: foo ^ ~ -> history, is typed",
   "foo ^ ~", [3, 10]],
  ["15: foo * # -> is star, in title",
   "foo * #", [5, 7, 8, 9, 10, 11]],
  ["16: foo * @ -> is star, in url",
   "foo * @", [6, 7, 10, 11]],
  ["17: foo * + -> same as +",
   "foo * +", [8, 9, 10, 11]],
  ["17.1: foo * ~ -> is star, is typed",
   "foo * ~", [10]],
  ["18: foo # @ -> in title, in url",
   "foo # @", [3, 7, 10, 11]],
  ["19: foo # + -> in title, is tag",
   "foo # +", [8, 9, 10, 11]],
  ["19.1: foo # ~ -> in title, is typed",
   "foo # ~", [3, 10]],
  ["20: foo @ + -> in url, is tag",
   "foo @ +", [10, 11]],
  ["20.1: foo @ ~ -> in url, is typed",
   "foo @ ~", [3, 10]],
  ["20.2: foo + ~ -> is tag, is typed",
   "foo + ~", [10]],

  // Test default usage by setting certain bits of default.behavior to 1
  ["21: foo -> default history",
   "foo", [1, 2, 3, 5, 10], function () { setPref({ history: true }); }],
  ["22: foo -> default history or is star",
   "foo", [1, 2, 3, 5, 6, 7, 8, 9, 10, 11], () => setPref({ history: true, bookmark: true })],
  ["22.1: foo -> default history or is star, is typed",
   "foo", [3, 10], () => setPref({ history: true, bookmark: true, "history.onlyTyped": true })],

];

function setPref(aTypes) {
  clearSuggestPrefs();
  for (let type in aTypes) {
    prefs.setBoolPref("browser.urlbar.suggest." + type, aTypes[type]);
  }
}

function clearSuggestPrefs() {
  prefs.setBoolPref("browser.urlbar.suggest.history", false);
  prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  prefs.setBoolPref("browser.urlbar.suggest.history.onlyTyped", false);
  prefs.setBoolPref("browser.urlbar.suggest.openpage", false);
}

function changeRestrict(aType, aChar)
{
  let branch = "browser.urlbar.";
  // "title" and "url" are different from everything else, so special case them.
  if (aType == "title" || aType == "url")
    branch += "match.";
  else
    branch += "restrict.";

  print("changing restrict for " + aType + " to '" + aChar + "'");
  prefs.setCharPref(branch + aType, aChar);
}

function resetRestrict(aType)
{
  let branch = "browser.urlbar.";
  // "title" and "url" are different from everything else, so special case them.
  if (aType == "title" || aType == "url")
    branch += "match.";
  else
    branch += "restrict.";

  if (prefs.prefHasUserValue(branch + aType))
    prefs.clearUserPref(branch + aType);
}

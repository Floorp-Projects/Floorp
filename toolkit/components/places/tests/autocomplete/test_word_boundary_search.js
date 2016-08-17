/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 393678 to make sure matches against the url, title, tags are only
 * made on word boundaries instead of in the middle of words.
 *
 * Make sure we don't try matching one after a CamelCase because the upper-case
 * isn't really a word boundary. (bug 429498)
 *
 * Bug 429531 provides switching between "must match on word boundary" and "can
 * match," so leverage "must match" pref for checking word boundary logic and
 * make sure "can match" matches anywhere.
 */

var katakana = ["\u30a8", "\u30c9"]; // E, Do
var ideograph = ["\u4efb", "\u5929", "\u5802"]; // Nin Ten Do

// Define some shared uris and titles (each page needs its own uri)
var kURIs = [
  "http://matchme/",
  "http://dontmatchme/",
  "http://title/1",
  "http://title/2",
  "http://tag/1",
  "http://tag/2",
  "http://crazytitle/",
  "http://katakana/",
  "http://ideograph/",
  "http://camel/pleaseMatchMe/",
];
var kTitles = [
  "title1",
  "matchme2",
  "dontmatchme3",
  "!@#$%^&*()_+{}|:<>?word",
  katakana.join(""),
  ideograph.join(""),
];

// Boundaries on the url
addPageBook(0, 0);
addPageBook(1, 0);
// Boundaries on the title
addPageBook(2, 1);
addPageBook(3, 2);
// Boundaries on the tag
addPageBook(4, 0, 0, [1]);
addPageBook(5, 0, 0, [2]);
// Lots of word boundaries before a word
addPageBook(6, 3);
// Katakana
addPageBook(7, 4);
// Ideograph
addPageBook(8, 5);
// CamelCase
addPageBook(9, 0);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
var gTests = [
  // Tests after this one will match only on word boundaries
  ["0: Match 'match' at the beginning or after / or on a CamelCase",
   "match", [0, 2, 4, 9],
   () => setBehavior(2)],
  ["1: Match 'dont' at the beginning or after /",
   "dont", [1, 3, 5]],
  ["2: Match '2' after the slash and after a word (in tags too)",
   "2", [2, 3, 4, 5]],
  ["3: Match 't' at the beginning or after /",
   "t", [0, 1, 2, 3, 4, 5, 9]],
  ["4: Match 'word' after many consecutive word boundaries",
   "word", [6]],
  ["5: Match a word boundary '/' for everything",
   "/", [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]],
  ["6: Match word boundaries '()_+' that are among word boundaries",
   "()_+", [6]],

  ["7: Katakana characters form a string, so match the beginning",
   katakana[0], [7]],
  /*["8: Middle of a katakana word shouldn't be matched",
   katakana[1], []],*/

  ["9: Ideographs are treated as words so 'nin' is one word",
   ideograph[0], [8]],
  ["10: Ideographs are treated as words so 'ten' is another word",
   ideograph[1], [8]],
  ["11: Ideographs are treated as words so 'do' is yet another",
   ideograph[2], [8]],

  ["12: Extra negative assert that we don't match in the middle",
   "ch", []],
  ["13: Don't match one character after a camel-case word boundary (bug 429498)",
   "atch", []],

  // Tests after this one will match against word boundaries and anywhere
  ["14: Match on word boundaries as well as anywhere (bug 429531)",
   "tch", [0, 1, 2, 3, 4, 5, 9],
   () => setBehavior(1)],
];

function setBehavior(aType) {
  prefs.setIntPref("browser.urlbar.matchBehavior", aType);
}

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places Test Code.
 *
 * The Initial Developer of the Original Code is
 * Edward Lee <edward.lee@engineering.uiuc.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

let katakana = ["\u30a8", "\u30c9"]; // E, Do
let ideograph = ["\u4efb", "\u5929", "\u5802"]; // Nin Ten Do

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
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
let kTitles = [
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
let gTests = [
  // Tests after this one will match only on word boundaries
  ["0: Match 'match' at the beginning or after / or on a CamelCase",
   "match", [0,2,4,9],
   function() setBehavior(2)],
  ["1: Match 'dont' at the beginning or after /",
   "dont", [1,3,5]],
  ["2: Match '2' after the slash and after a word (in tags too)",
   "2", [2,3,4,5]],
  ["3: Match 't' at the beginning or after /",
   "t", [0,1,2,3,4,5,9]],
  ["4: Match 'word' after many consecutive word boundaries",
   "word", [6]],
  ["5: Match a word boundary '/' for everything",
   "/", [0,1,2,3,4,5,6,7,8,9]],
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
   "tch", [0,1,2,3,4,5,9],
   function() setBehavior(1)],
];

function setBehavior(aType) {
  prefs.setIntPref("browser.urlbar.matchBehavior", aType);
}

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
 * Test for bug 395161 that allows special searches that restrict results to
 * history/bookmark/tagged items and title/url matches.
 */

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
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
let kTitles = [
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
for each (let uri in [4,6,7,8,9,11])
  histsvc.removePage(toURI(kURIs[uri]));

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
let gTests = [
  // Test restricting searches
  ["0: History restrict",
   "^", [0,1,2,3,5,10]],
  ["1: Star restrict",
   "*", [4,5,6,7,8,9,10,11]],
  ["2: Tag restrict",
   "+", [8,9,10,11]],

  // Test specials as any word position
  ["3: Special as first word",
   "^ foo bar", [1,2,3,5,10]],
  ["4: Special as middle word",
   "foo ^ bar", [1,2,3,5,10]],
  ["5: Special as last word",
   "foo bar ^", [1,2,3,5,10]],

  // Test restricting and matching searches with a term
  ["6: foo ^ -> history",
   "foo ^", [1,2,3,5,10]],
  ["7: foo * -> is star",
   "foo *", [5,6,7,8,9,10,11]],
  ["8: foo # -> in title",
   "foo #", [1,3,5,7,8,9,10,11]],
  ["9: foo @ -> in url",
   "foo @", [2,3,6,7,10,11]],
  ["10: foo + -> is tag",
   "foo +", [8,9,10,11]],

  // Test various pairs of special searches
  ["11: foo ^ * -> history, is star",
   "foo ^ *", [5,10]],
  ["12: foo ^ # -> history, in title",
   "foo ^ #", [1,3,5,10]],
  ["13: foo ^ @ -> history, in url",
   "foo ^ @", [2,3,10]],
  ["14: foo ^ + -> history, is tag",
   "foo ^ +", [10]],
  ["15: foo * # -> is star, in title",
   "foo * #", [5,7,8,9,10,11]],
  ["16: foo * @ -> is star, in url",
   "foo * @", [6,7,10,11]],
  ["17: foo * + -> same as +",
   "foo * +", [8,9,10,11]],
  ["18: foo # @ -> in title, in url",
   "foo # @", [3,7,10,11]],
  ["19: foo # + -> in title, is tag",
   "foo # +", [8,9,10,11]],
  ["20: foo @ + -> in url, is tag",
   "foo @ +", [10,11]],

  // Test default usage by setting certain bits of default.behavior to 1
  ["21: foo -> default history",
   "foo", [1,2,3,5,10], function() makeDefault(1)],
  ["22: foo -> default history, is star",
   "foo", [5,10], function() makeDefault(3)],
  ["23: foo -> default history, is star, in url",
   "foo", [10], function() makeDefault(19)],

  // Change the default to be less restrictive to make sure we find more
  ["24: foo -> default is star, in url",
   "foo", [6,7,10,11], function() makeDefault(18)],
  ["25: foo -> default in url",
   "foo", [2,3,6,7,10,11], function() makeDefault(16)],
];

function makeDefault(aDefault) {
  prefs.setIntPref("browser.urlbar.default.behavior", aDefault);
}

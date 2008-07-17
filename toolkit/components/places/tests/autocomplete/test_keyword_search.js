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
  otherBase + "%s",
  keyBase + "twoKey",
  otherBase + "twoKey",
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

  // This adds a second keyword so anything after this will match 2 keywords
  ["5: Two keywords matched",
   keyKey + " twoKey", [7,8],
   function() {
     // Add the keyword search as well as search results
     addPageBook(6, 0, 1, [], keyKey);
     gPages[7] = [7,1];
     gPages[8] = [8,1];
   }],
];

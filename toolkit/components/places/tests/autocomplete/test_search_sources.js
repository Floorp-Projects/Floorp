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
 * Test for bug 463661 to allow configuring the autocomplete to search history,
 * bookmarks, both, or neither. Also test bug 463535 for pref changing search.
 */

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
  "http://url/0",
  "http://url/1",
  "http://url/2",
];
let kTitles = [
  "title",
];

addPageBook(0, 0); // visited page
addPageBook(1, 0, 0); // visited bookmark
addPageBook(2, 0, 0); // unvisited bookmark

// Remove the visit for this bookmark
histsvc.removePage(toURI(kURIs[2]));

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
let gTests = [
  // We get more results as we become less restrictive with the same search
  ["1: search none",
   "url", [], function() setSearch(0)],
  ["2: search history",
   "url", [0,1], function() setSearch(1)],
  ["3: search bookmark",
   "url", [1,2], function() setSearch(2)],
  ["4: search both",
   "url", [0,1,2], function() setSearch(3)],
];

function setSearch(aSearch) {
  prefs.setIntPref("browser.urlbar.search.sources", aSearch);
}

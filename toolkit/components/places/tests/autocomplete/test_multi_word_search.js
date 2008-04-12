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

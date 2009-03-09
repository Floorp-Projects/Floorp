/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
   vim:set ts=2 sw=2 sts=2 et:
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Places Test code.
 *
 * The Initial Developer of the Original Code is Mozilla Corp.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Drew Willcoxon <adw@mozilla.com> (Original Author)
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
 * Bug 479082
 * https://bugzilla.mozilla.org/show_bug.cgi?id=479082
 *
 * Ensures that unvisited livemarks that are not bookmarked elsewhere do not
 * show up in AutoComplete results.  Also does some related checking that
 * visited and/or separately bookmarked livemarks *do* show up.
 */

let kURIs = [
  "http://example.com/",
  "http://example.com/container",
  // URIs that themselves should match our searches
  "http://example.com/unvisited-not-bookmarked-elsewhere/vy-__--D",
  "http://example.com/visited-not-bookmarked-elsewhere/_0YlX-9L",
  "http://example.com/unvisited-bookmarked-elsewhere/X132H20w",
  "http://example.com/visited-bookmarked-elsewhere/n_6D_Pw5",
  // URIs whose titles should match our searches
  "http://example.com/unvisited-not-bookmarked-elsewhere",
  "http://example.com/visited-not-bookmarked-elsewhere",
  "http://example.com/unvisited-bookmarked-elsewhere",
  "http://example.com/visited-bookmarked-elsewhere",
];
let kTitles = [
  "container title",
  // titles for URIs that should match our searches
  "unvisited not-bookmarked-elsewhere child title",
  "visited   not-bookmarked-elsewhere child title",
  "unvisited bookmarked-elsewhere     child title",
  "visited   bookmarked-elsewhere     child title",
  // titles that themselves should match our searches
  "unvisited not-bookmarked-elsewhere child title P-13U-z-",
  "visited   not-bookmarked-elsewhere child title _3-X4_Qd",
  "unvisited bookmarked-elsewhere     child title I4jOt6o4",
  "visited   bookmarked-elsewhere     child title 9-RVT4D5",
];

// our searches will match URIs on these livemarks
addLivemark(0, 1, 0, 2, 1, null, true);
addLivemark(0, 1, 0, 3, 2, null, false);
addLivemark(0, 1, 0, 4, 3, null, true);
addPageBook(4, 3, 3, null, null, null, true);
addLivemark(0, 1, 0, 5, 4, null, false);

// we'll match titles on these livemarks
addLivemark(0, 1, 0, 6, 5, null, true);
addLivemark(0, 1, 0, 7, 6, null, false);
addLivemark(0, 1, 0, 8, 7, null, true);
addPageBook(8, 7, 7, null, null, null, true);
addLivemark(0, 1, 0, 9, 8, null, false);

let gTests = [
  // URIs should match these searches
  ["0: unvisited not-bookmarked-elsewhere livemark child (should be empty)",
   "vy-__--D", []],
  ["1: visited not-bookmarked-elsewhere livemark child (should not be empty)",
   "_0YlX-9L", [3]],
  ["2: unvisited bookmarked-elsewhere livemark child (should not be empty)",
   "X132H20w", [4]],
  ["3: visited bookmarked-elsewhere livemark child (should not be empty)",
   "n_6D_Pw5", [5]],
  // titles should match these
  ["4: unvisited not-bookmarked-elsewhere livemark child (should be empty)",
   "P-13U-z-", []],
  ["5: visited not-bookmarked-elsewhere livemark child (should not be empty)",
   "_3-X4_Qd", [7]],
  ["6: unvisited bookmarked-elsewhere livemark child (should not be empty)",
   "I4jOt6o4", [8]],
  ["7: visited bookmarked-elsewhere livemark child (should not be empty)",
   "9-RVT4D5", [9]],
];

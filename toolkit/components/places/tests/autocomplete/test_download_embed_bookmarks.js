/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
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
 * The Original Code is Places Test Code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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
 * Tests bug 449406 to ensure that TRANSITION_DOWNLOAD and TRANSITION_EMBED
 * bookmarked uri's show up in the location bar.
 */

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
  "http://download/bookmarked",
  "http://embed/bookmarked",
  "http://download",
  "http://embed",
];
let kTitles = [
  "download-bookmark",
  "embed-bookmark",
  "download2",
  "embed2",
];

// Add download and embed uris
addPageBook(0, 0, 0, undefined, undefined, TRANSITION_DOWNLOAD);
addPageBook(1, 1, 1, undefined, undefined, TRANSITION_EMBED);
addPageBook(2, 2, undefined, undefined, undefined, TRANSITION_DOWNLOAD);
addPageBook(3, 3, undefined, undefined, undefined, TRANSITION_EMBED);

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
let gTests = [
  ["0: Searching for bookmarked download uri matches",
   kTitles[0], [0]],
  ["1: Searching for bookmarked embed uri matches",
   kTitles[1], [1]],
  ["2: Searching for download uri does not match",
   kTitles[2], []],
  ["3: Searching for embed uri does not match",
   kTitles[3], []],
];

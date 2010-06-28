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
 * Test bug 424717 to make sure searching with an existing location like
 * http://site/ also matches https://site/ or ftp://site/. Same thing for
 * ftp://site/ and https://site/.
 *
 * Test bug 461483 to make sure a search for "w" doesn't match the "www." from
 * site subdomains.
 */

// Define some shared uris and titles (each page needs its own uri)
let kURIs = [
  "http://www.site/",
  "http://site/",
  "ftp://ftp.site/",
  "ftp://site/",
  "https://www.site/",
  "https://site/",
  "http://woohoo/",
  "http://wwwwwwacko/",
];
let kTitles = [
  "title",
];

// Add various protocols of site
addPageBook(0, 0);
addPageBook(1, 0);
addPageBook(2, 0);
addPageBook(3, 0);
addPageBook(4, 0);
addPageBook(5, 0);
addPageBook(6, 0);
addPageBook(7, 0);

let allSite = [0,1,2,3,4,5];

// Provide for each test: description; search terms; array of gPages indices of
// pages that should match; optional function to be run before the test
let gTests = [
  ["0: http://www.site matches all site", "http://www.site", allSite],
  ["1: http://site matches all site", "http://site", allSite],
  ["2: ftp://ftp.site matches itself", "ftp://ftp.site", [2]],
  ["3: ftp://site matches all site", "ftp://site", allSite],
  ["4: https://www.site matches all site", "https://www.site", allSite],
  ["5: https://site matches all site", "https://site", allSite],
  ["6: www.site matches all site", "www.site", allSite],

  ["7: w matches none of www.", "w", [6,7]],
  ["8: http://w matches none of www.", "w", [6,7]],
  ["9: http://www.w matches none of www.", "w", [6,7]],

  ["10: ww matches none of www.", "ww", [7]],
  ["11: http://ww matches none of www.", "http://ww", [7]],
  ["12: http://www.ww matches none of www.", "http://www.ww", [7]],

  ["13: www matches none of www.", "www", [7]],
  ["14: http://www matches none of www.", "http://www", [7]],
  ["15: http://www.www matches none of www.", "http://www.www", [7]],
];

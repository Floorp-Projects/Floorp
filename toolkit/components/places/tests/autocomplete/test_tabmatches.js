/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gTabRestrictChar = prefs.getCharPref("browser.urlbar.restrict.openpage");

let kSearchParam = "enable-actions";

let kURIs = [
  "http://abc.com/",
  "moz-action:switchtab,http://abc.com/",
  "http://xyz.net/",
  "moz-action:switchtab,http://xyz.net/",
  "about:mozilla",
  "moz-action:switchtab,about:mozilla",
  "data:text/html,test",
  "moz-action:switchtab,data:text/html,test"
];

let kTitles = [
  "ABC rocks",
  "xyz.net - we're better than ABC",
  "about:mozilla",
  "data:text/html,test"
];

addPageBook(0, 0);
gPages[1] = [1, 0];
addPageBook(2, 1);
gPages[3] = [3, 1];

addOpenPages(0, 1);

// PAges that cannot be registered in history.
addOpenPages(4, 1);
gPages[5] = [5, 2];
addOpenPages(6, 1);
gPages[7] = [7, 3];

let gTests = [
  ["0: single result, that is also a tab match",
   "abc.com", [1]],
  ["1: two results, one tab match",
   "abc", [1,2]],
  ["2: two results, both tab matches",
   "abc", [1,3],
   function() {
     addOpenPages(2, 1);
   }],
  ["3: two results, both tab matches, one has multiple tabs",
   "abc", [1,3],
   function() {
     addOpenPages(2, 5);
   }],
  ["4: two results, no tab matches",
   "abc", [0,2],
   function() {
     removeOpenPages(0, 1);
     removeOpenPages(2, 6);
   }],
  ["5: tab match search with restriction character",
   gTabRestrictChar + " abc", [1],
   function() {
    addOpenPages(0, 1);
   }],
  ["6: tab match with not-addable pages",
   "mozilla", [5]],
  ["7: tab match with not-addable pages and restriction character",
   gTabRestrictChar + " mozilla", [5]],
  ["8: tab match with not-addable pages and only restriction character",
   gTabRestrictChar, [1, 5, 7]],
];


function addOpenPages(aUri, aCount) {
  let num = aCount || 1;
  let acprovider = Cc["@mozilla.org/autocomplete/search;1?name=history"].
                   getService(Ci.mozIPlacesAutoComplete);
  for (let i = 0; i < num; i++) {
    acprovider.registerOpenPage(toURI(kURIs[aUri]));
  }
}

function removeOpenPages(aUri, aCount) {
  let num = aCount || 1;
  let acprovider = Cc["@mozilla.org/autocomplete/search;1?name=history"].
                   getService(Ci.mozIPlacesAutoComplete);
  for (let i = 0; i < num; i++) {
    acprovider.unregisterOpenPage(toURI(kURIs[aUri]));
  }
}

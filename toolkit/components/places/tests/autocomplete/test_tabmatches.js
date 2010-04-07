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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blair McBride <bmcbride@mozilla.com> (Original Author)
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

let gTabRestrictChar = prefs.getCharPref("browser.urlbar.restrict.openpage");

let kSearchParam = "enable-actions";

let kURIs = [
  "http://abc.com/",
  "moz-action:switchtab,http://abc.com/",
  "http://xyz.net/",
  "moz-action:switchtab,http://xyz.net/"
];

let kTitles = [
  "ABC rocks",
  "xyz.net - we're better than ABC"
];

addPageBook(0, 0);
gPages[1] = [1, 0];
addPageBook(2, 1);
gPages[3] = [3, 1];

addOpenPages(0, 1);


let gTests = [
  ["0: single result, that is also a tab match",
   "abc.com", [0,1]],
  ["1: two results, one tab match",
   "abc", [0,1,2]],
  ["2: two results, both tab matches",
   "abc", [0,1,2,3],
   function() {
     addOpenPages(2, 1);
   }],
  ["3: two results, both tab matches, one has multiple tabs",
   "abc", [0,1,2,3],
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
   }]
];


function addOpenPages(aUri, aCount) {
  let num = aCount || 1;
  for (let i = 0; i < num; i++) {
    bhist.registerOpenPage(toURI(kURIs[aUri]));
  }
}

function removeOpenPages(aUri, aCount) {
  let num = aCount || 1;
  for (let i = 0; i < num; i++) {
    bhist.unregisterOpenPage(toURI(kURIs[aUri]));
  }
}

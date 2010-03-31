/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77bonardo.net> (Original Author)
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

let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
let bh = hs.QueryInterface(Ci.nsIBrowserHistory);
let db = hs.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;

const URLS = [
  { u: "http://www.google.com/search?q=testing%3Bthis&ie=utf-8&oe=utf-8&aq=t&rls=org.mozilla:en-US:unofficial&client=firefox-a",
    s: "goog" },
];

function run_test() {
  URLS.forEach(test_url);
}

function test_url(aURL) {
  print("Testing url: " + aURL.u);
  hs.addVisit(uri(aURL.u), Date.now() * 1000, null, hs.TRANSITION_TYPED, false, 0);
  let query = hs.getNewQuery();
  query.searchTerms = aURL.s;
  let options = hs.getNewQueryOptions();
  let root = hs.executeQuery(query, options).root;
  root.containerOpen = true;
  let cc = root.childCount;
  do_check_eq(cc, 1);
  print("Checking url is in the query.");
  let node = root.getChild(0);
  print("Found " + node.uri);
  root.containerOpen = false;
  bh.removePage(uri(node.uri));
}

function check_empty_table(table_name) {
  print("Checking url has been removed.");
  let stmt = db.createStatement("SELECT count(*) FROM " + table_name);
  try {
    stmt.executeStep();
    do_check_eq(stmt.getInt32(0), 0);
  }
  finally {
    stmt.finalize();
  }
}

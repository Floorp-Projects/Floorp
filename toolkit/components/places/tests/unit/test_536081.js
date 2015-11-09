/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var bh = hs.QueryInterface(Ci.nsIBrowserHistory);
var db = hs.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;

const URLS = [
  { u: "http://www.google.com/search?q=testing%3Bthis&ie=utf-8&oe=utf-8&aq=t&rls=org.mozilla:en-US:unofficial&client=firefox-a",
    s: "goog" },
];

function run_test()
{
  run_next_test();
}

add_task(function* test_execute()
{
  for (let url of URLS) {
    yield task_test_url(url);
  }
});

function* task_test_url(aURL) {
  print("Testing url: " + aURL.u);
  yield PlacesTestUtils.addVisits(uri(aURL.u));
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

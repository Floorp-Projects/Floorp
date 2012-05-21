/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Need to test that removing a page from autocomplete actually removes a page
 * Description From  Shawn Wilsher :sdwilsh   2009-02-18 11:29:06 PST
 * We don't test the code path of onValueRemoved
 * for the autocomplete implementation
 * Bug 479089
 */

var ios = Cc["@mozilla.org/network/io-service;1"].
getService(Components.interfaces.nsIIOService);

var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
getService(Ci.nsINavHistoryService);

function run_test()
{
  // QI to nsIAutoCompleteSimpleResultListener
  var listener = Cc["@mozilla.org/autocomplete/search;1?name=history"].
                 getService(Components.interfaces.nsIAutoCompleteSimpleResultListener);

  // add history visit
  var now = Date.now() * 1000;
  var uri = ios.newURI("http://foo.mozilla.com/", null, null);
  var ref = ios.newURI("http://mozilla.com/", null, null);
  var visit = histsvc.addVisit(uri, now, ref, 1, false, 0);
  // create a query object
  var query = histsvc.getNewQuery();
  // create the options object we will never use
  var options = histsvc.getNewQueryOptions();
  // look for this uri only
  query.uri = uri;
  // execute
  var queryRes = histsvc.executeQuery(query, options);
  // open the result container
  queryRes.root.containerOpen = true;
  // debug queries
  // dump_table("moz_places");
  do_check_eq(queryRes.root.childCount, 1);  
  // call the untested code path
  listener.onValueRemoved(null, uri.spec, true);
  // make sure it is GONE from the DB
  do_check_eq(queryRes.root.childCount, 0);
  // close the container
  queryRes.root.containerOpen = false;
}


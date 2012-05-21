/* -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);

function run_test() {
  var query = hs.getNewQuery();
  var options = hs.getNewQueryOptions();
  options.resultType = options.RESULT_TYPE_QUERY;
  var result = hs.executeQuery(query, options);
  result.root.containerOpen = true;
  var rootNode = result.root;
  rootNode.QueryInterface(Ci.nsINavHistoryQueryResultNode);
  var queries = rootNode.getQueries();
  do_check_eq(queries[0].uri, null); // Should be null, instead of crashing the browser
  rootNode.containerOpen = false;
}

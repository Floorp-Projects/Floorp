/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// AddURI transactions are created lazily.
do_test_pending();

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

var ghist = Cc["@mozilla.org/browser/global-history;2"].getService(Ci.nsIGlobalHistory2);

// create and add history observer
var observer = {
  onBeginUpdateBatch: function() {},
  onEndUpdateBatch: function() {},
  visitCount: 0,
  onVisit: function(aURI, aVisitID, aTime, aSessionID, aReferringID, aTransitionType) {
    this.visitCount++;
    dump("onVisit: " + aURI.spec + "\n");
    confirm_results();
  },
  onTitleChanged: function () {},
  onBeforeDeleteURI: function () {},
  onDeleteURI: function () {},
  onClearHistory: function () {},
  onPageChanged: function () {},
  onDeleteVisits: function () {},
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavHistoryObserver
  ])
};

histsvc.addObserver(observer, false);

// main
function run_test() {
  var now = Date.now();
  var testURI = uri("http://fez.com");
  ghist.addURI(testURI, false, true, null);
  ghist.addURI(testURI, false, true, testURI);
  // lazy message timer is 3000, see LAZY_MESSAGE_TIMEOUT
  do_timeout(3500, confirm_results);
}

function confirm_results() {
  var options = histsvc.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_VISIT;
  options.includeHidden = true;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  do_check_eq(cc, 1);
  root.containerOpen = false;
  do_test_finished();
}

/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

var gVisits = [{url: "http://www.mozilla.com/",
                transition: histsvc.TRANSITION_TYPED},
               {url: "http://www.google.com/", 
                transition: histsvc.TRANSITION_BOOKMARK},
               {url: "http://www.espn.com/",
                transition: histsvc.TRANSITION_LINK}];

function run_test()
{
  run_next_test();
}

add_task(function test_execute()
{
  for each (var visit in gVisits) {
    if (visit.transition == histsvc.TRANSITION_TYPED)
      histsvc.markPageAsTyped(uri(visit.url));
    else if (visit.transition == histsvc.TRANSITION_BOOKMARK)
      histsvc.markPageAsFollowedBookmark(uri(visit.url))
    else {
     // because it is a top level visit with no referrer,
     // it will result in TRANSITION_LINK
    }
    yield promiseAddVisits({uri: uri(visit.url),
                            transition: visit.transition});
  }

  do_test_pending();
});

// create and add history observer
var observer = {
  _visitCount: 0,
  onBeginUpdateBatch: function() {
  },
  onEndUpdateBatch: function() {
  },
  onVisit: function(aURI, aVisitID, aTime, aSessionID, 
                    aReferringID, aTransitionType, aAdded) {
    do_check_eq(aURI.spec, gVisits[this._visitCount].url);
    do_check_eq(aTransitionType, gVisits[this._visitCount].transition);
    this._visitCount++;

    if (this._visitCount == gVisits.length)
      do_test_finished();
  },
  onTitleChanged: function () {},
  onDeleteURI: function () {},
  onClearHistory: function () {},
  onPageChanged: function () {},
  onDeleteVisits: function () {},
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavHistoryObserver
  ])
};

histsvc.addObserver(observer, false);

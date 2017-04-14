/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gVisits = [{url: "http://www.mozilla.com/",
                transition: TRANSITION_TYPED},
               {url: "http://www.google.com/",
                transition: TRANSITION_BOOKMARK},
               {url: "http://www.espn.com/",
                transition: TRANSITION_LINK}];

function run_test() {
  run_next_test();
}

add_task(function* test_execute() {
  let observer;
  let completionPromise = new Promise(resolveCompletionPromise => {
    observer = {
      __proto__: NavHistoryObserver.prototype,
      _visitCount: 0,
      onVisit(aURI, aVisitID, aTime, aSessionID, aReferringID,
                        aTransitionType, aAdded) {
        do_check_eq(aURI.spec, gVisits[this._visitCount].url);
        do_check_eq(aTransitionType, gVisits[this._visitCount].transition);
        this._visitCount++;

        if (this._visitCount == gVisits.length) {
          resolveCompletionPromise();
        }
      },
    };
  });

  PlacesUtils.history.addObserver(observer, false);

  for (var visit of gVisits) {
    if (visit.transition == TRANSITION_TYPED)
      PlacesUtils.history.markPageAsTyped(uri(visit.url));
    else if (visit.transition == TRANSITION_BOOKMARK)
      PlacesUtils.history.markPageAsFollowedBookmark(uri(visit.url))
    else {
     // because it is a top level visit with no referrer,
     // it will result in TRANSITION_LINK
    }
    yield PlacesTestUtils.addVisits({
      uri: uri(visit.url),
      transition: visit.transition
    });
  }

  yield completionPromise;

  PlacesUtils.history.removeObserver(observer);
});


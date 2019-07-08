/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gVisits = [
  { url: "http://www.mozilla.com/", transition: TRANSITION_TYPED },
  { url: "http://www.google.com/", transition: TRANSITION_BOOKMARK },
  { url: "http://www.espn.com/", transition: TRANSITION_LINK },
];

add_task(async function test_execute() {
  let completionPromise = new Promise(resolveCompletionPromise => {
    let visitCount = 0;
    function listener(aEvents) {
      Assert.equal(aEvents.length, 1, "Right number of visits notified");
      Assert.equal(aEvents[0].type, "page-visited");
      let event = aEvents[0];
      Assert.equal(event.url, gVisits[visitCount].url);
      Assert.equal(event.transitionType, gVisits[visitCount].transition);
      visitCount++;

      if (visitCount == gVisits.length) {
        resolveCompletionPromise();
        PlacesObservers.removeListener(["page-visited"], listener);
      }
    }
    PlacesObservers.addListener(["page-visited"], listener);
  });

  for (var visit of gVisits) {
    if (visit.transition == TRANSITION_TYPED) {
      PlacesUtils.history.markPageAsTyped(uri(visit.url));
    } else if (visit.transition == TRANSITION_BOOKMARK) {
      PlacesUtils.history.markPageAsFollowedBookmark(uri(visit.url));
    } else {
      // because it is a top level visit with no referrer,
      // it will result in TRANSITION_LINK
    }
    await PlacesTestUtils.addVisits({
      uri: uri(visit.url),
      transition: visit.transition,
    });
  }

  await completionPromise;
});

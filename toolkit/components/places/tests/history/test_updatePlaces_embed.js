/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that updatePlaces properly handled callbacks for embed visits.

"use strict";

add_task(async function test_embed_visit() {
  let place = {
    uri: NetUtil.newURI("http://places.test/"),
    visits: [
      { transitionType: PlacesUtils.history.TRANSITIONS.EMBED,
        visitDate: PlacesUtils.toPRTime(new Date()) }
    ],
  };
  let errors = 0;
  let results = 0;
  let updated = await new Promise(resolve => {
    PlacesUtils.asyncHistory.updatePlaces(place, {
      ignoreErrors: true,
      ignoreResults: true,
      handleError(aResultCode, aPlace) {
        errors++;
      },
      handleResult(aPlace) {
        results++;
      },
      handleCompletion(resultCount) {
        resolve(resultCount);
      }
    });
  });
  Assert.equal(errors, 0, "There should be no error callback");
  Assert.equal(results, 0, "There should be no result callback");
  Assert.equal(updated, 1, "The visit should have been added");
});

add_task(async function test_misc_visits() {
  let place = {
    uri: NetUtil.newURI("http://places.test/"),
    visits: [
      { transitionType: PlacesUtils.history.TRANSITIONS.EMBED,
        visitDate: PlacesUtils.toPRTime(new Date()) },
      { transitionType: PlacesUtils.history.TRANSITIONS.LINK,
        visitDate: PlacesUtils.toPRTime(new Date()) }
    ],
  };
  let errors = 0;
  let results = 0;
  let updated = await new Promise(resolve => {
    PlacesUtils.asyncHistory.updatePlaces(place, {
      ignoreErrors: true,
      ignoreResults: true,
      handleError(aResultCode, aPlace) {
        errors++;
      },
      handleResult(aPlace) {
        results++;
      },
      handleCompletion(resultCount) {
        resolve(resultCount);
      }
    });
  });
  Assert.equal(errors, 0, "There should be no error callback");
  Assert.equal(results, 0, "There should be no result callback");
  Assert.equal(updated, 2, "The visit should have been added");
});

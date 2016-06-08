/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the async history API exposed by mozIAsyncHistory.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

const TEST_DOMAIN = "http://mozilla.org/";
const URI_VISIT_SAVED = "uri-visit-saved";
const RECENT_EVENT_THRESHOLD = 15 * 60 * 1000000;

////////////////////////////////////////////////////////////////////////////////
//// Helpers
/**
 * Object that represents a mozIVisitInfo object.
 *
 * @param [optional] aTransitionType
 *        The transition type of the visit.  Defaults to TRANSITION_LINK if not
 *        provided.
 * @param [optional] aVisitTime
 *        The time of the visit.  Defaults to now if not provided.
 */
function VisitInfo(aTransitionType,
                   aVisitTime) {
  this.transitionType =
    aTransitionType === undefined ? TRANSITION_LINK : aTransitionType;
  this.visitDate = aVisitTime || Date.now() * 1000;
}

function promiseUpdatePlaces(aPlaces) {
  return new Promise((resolve, reject) => {
    PlacesUtils.asyncHistory.updatePlaces(aPlaces, {
      _errors: [],
      _results: [],
      handleError(aResultCode, aPlace) {
        this._errors.push({ resultCode: aResultCode, info: aPlace});
      },
      handleResult(aPlace) {
        this._results.push(aPlace);
      },
      handleCompletion() {
        resolve({ errors: this._errors, results: this._results });
      }
    });
  });
}

/**
 * Listens for a title change notification, and calls aCallback when it gets it.
 *
 * @param aURI
 *        The URI of the page we expect a notification for.
 * @param aExpectedTitle
 *        The expected title of the URI we expect a notification for.
 * @param aCallback
 *        The method to call when we have gotten the proper notification about
 *        the title changing.
 */
function TitleChangedObserver(aURI,
                              aExpectedTitle,
                              aCallback) {
  this.uri = aURI;
  this.expectedTitle = aExpectedTitle;
  this.callback = aCallback;
}
TitleChangedObserver.prototype = {
  __proto__: NavHistoryObserver.prototype,
  onTitleChanged(aURI, aTitle, aGUID) {
    do_print("onTitleChanged(" + aURI.spec + ", " + aTitle + ", " + aGUID + ")");
    if (!this.uri.equals(aURI)) {
      return;
    }
    do_check_eq(aTitle, this.expectedTitle);
    do_check_guid_for_uri(aURI, aGUID);
    this.callback();
  },
};

/**
 * Listens for a visit notification, and calls aCallback when it gets it.
 *
 * @param aURI
 *        The URI of the page we expect a notification for.
 * @param aCallback
 *        The method to call when we have gotten the proper notification about
 *        being visited.
 */
function VisitObserver(aURI,
                       aGUID,
                       aCallback)
{
  this.uri = aURI;
  this.guid = aGUID;
  this.callback = aCallback;
}
VisitObserver.prototype = {
  __proto__: NavHistoryObserver.prototype,
  onVisit: function(aURI,
                    aVisitId,
                    aTime,
                    aSessionId,
                    aReferringId,
                    aTransitionType,
                    aGUID)
  {
    do_print("onVisit(" + aURI.spec + ", " + aVisitId + ", " + aTime +
             ", " + aSessionId + ", " + aReferringId + ", " +
             aTransitionType + ", " + aGUID + ")");
    if (!this.uri.equals(aURI) || this.guid != aGUID) {
      return;
    }
    this.callback(aTime, aTransitionType);
  },
};

/**
 * Tests that a title was set properly in the database.
 *
 * @param aURI
 *        The uri to check.
 * @param aTitle
 *        The expected title in the database.
 */
function do_check_title_for_uri(aURI,
                                aTitle)
{
  let stack = Components.stack.caller;
  let stmt = DBConn().createStatement(
    `SELECT title
     FROM moz_places
     WHERE url = :url`
  );
  stmt.params.url = aURI.spec;
  do_check_true(stmt.executeStep(), stack);
  do_check_eq(stmt.row.title, aTitle, stack);
  stmt.finalize();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

add_task(function* test_interface_exists() {
  let history = Cc["@mozilla.org/browser/history;1"].getService(Ci.nsISupports);
  do_check_true(history instanceof Ci.mozIAsyncHistory);
});

add_task(function* test_invalid_uri_throws() {
  // First, test passing in nothing.
  let place = {
    visits: [
      new VisitInfo(),
    ],
  };
  try {
    yield promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }

  // Now, test other bogus things.
  const TEST_VALUES = [
    null,
    undefined,
    {},
    [],
    TEST_DOMAIN + "test_invalid_id_throws",
  ];
  for (let i = 0; i < TEST_VALUES.length; i++) {
    place.uri = TEST_VALUES[i];
    try {
      yield promiseUpdatePlaces(place);
      do_throw("Should have thrown!");
    }
    catch (e) {
      do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
    }
  }
});

add_task(function* test_invalid_places_throws() {
  // First, test passing in nothing.
  try {
    PlacesUtils.asyncHistory.updatePlaces();
    do_throw("Should have thrown!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_XPC_NOT_ENOUGH_ARGS);
  }

  // Now, test other bogus things.
  const TEST_VALUES = [
    null,
    undefined,
    {},
    [],
    "",
  ];
  for (let i = 0; i < TEST_VALUES.length; i++) {
    let value = TEST_VALUES[i];
    try {
      yield promiseUpdatePlaces(value);
      do_throw("Should have thrown!");
    }
    catch (e) {
      do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
    }
  }
});

add_task(function* test_invalid_guid_throws() {
  // First check invalid length guid.
  let place = {
    guid: "BAD_GUID",
    uri: NetUtil.newURI(TEST_DOMAIN + "test_invalid_guid_throws"),
    visits: [
      new VisitInfo(),
    ],
  };
  try {
    yield promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }

  // Now check invalid character guid.
  place.guid = "__BADGUID+__";
  do_check_eq(place.guid.length, 12);
  try {
    yield promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_no_visits_throws() {
  const TEST_URI =
    NetUtil.newURI(TEST_DOMAIN + "test_no_id_or_guid_no_visits_throws");
  const TEST_GUID = "_RANDOMGUID_";
  const TEST_PLACEID = 2;

  let log_test_conditions = function(aPlace) {
    let str = "Testing place with " +
      (aPlace.uri ? "uri" : "no uri") + ", " +
      (aPlace.guid ? "guid" : "no guid") + ", " +
      (aPlace.visits ? "visits array" : "no visits array");
    do_print(str);
  };

  // Loop through every possible case.  Note that we don't actually care about
  // the case where we have no uri, place id, or guid (covered by another test),
  // but it is easier to just make sure it too throws than to exclude it.
  let place = { };
  for (let uri = 1; uri >= 0; uri--) {
    place.uri = uri ? TEST_URI : undefined;

    for (let guid = 1; guid >= 0; guid--) {
      place.guid = guid ? TEST_GUID : undefined;

      for (let visits = 1; visits >= 0; visits--) {
        place.visits = visits ? [] : undefined;

        log_test_conditions(place);
        try {
          yield promiseUpdatePlaces(place);
          do_throw("Should have thrown!");
        }
        catch (e) {
          do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
        }
      }
    }
  }
});

add_task(function* test_add_visit_no_date_throws() {
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_add_visit_no_date_throws"),
    visits: [
      new VisitInfo(),
    ],
  };
  delete place.visits[0].visitDate;
  try {
    yield promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_add_visit_no_transitionType_throws() {
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_add_visit_no_transitionType_throws"),
    visits: [
      new VisitInfo(),
    ],
  };
  delete place.visits[0].transitionType;
  try {
    yield promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_add_visit_invalid_transitionType_throws() {
  // First, test something that has a transition type lower than the first one.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN +
                        "test_add_visit_invalid_transitionType_throws"),
    visits: [
      new VisitInfo(TRANSITION_LINK - 1),
    ],
  };
  try {
    yield promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }

  // Now, test something that has a transition type greater than the last one.
  place.visits[0] = new VisitInfo(TRANSITION_RELOAD + 1);
  try {
    yield promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(function* test_non_addable_uri_errors() {
  // Array of protocols that nsINavHistoryService::canAddURI returns false for.
  const URLS = [
    "about:config",
    "imap://cyrus.andrew.cmu.edu/archive.imap",
    "news://new.mozilla.org/mozilla.dev.apps.firefox",
    "mailbox:Inbox",
    "moz-anno:favicon:http://mozilla.org/made-up-favicon",
    "view-source:http://mozilla.org",
    "chrome://browser/content/browser.xul",
    "resource://gre-resources/hiddenWindow.html",
    "data:,Hello%2C%20World!",
    "wyciwyg:/0/http://mozilla.org",
    "javascript:alert('hello wolrd!');",
    "blob:foo",
  ];
  let places = [];
  URLS.forEach(function(url) {
    try {
      let place = {
        uri: NetUtil.newURI(url),
        title: "test for " + url,
        visits: [
          new VisitInfo(),
        ],
      };
      places.push(place);
    }
    catch (e) {
      if (e.result != Cr.NS_ERROR_FAILURE) {
        throw e;
      }
      // NetUtil.newURI() can throw if e.g. our app knows about imap://
      // but the account is not set up and so the URL is invalid for us.
      // Note this in the log but ignore as it's not the subject of this test.
      do_print("Could not construct URI for '" + url + "'; ignoring");
    }
  });

  let placesResult = yield promiseUpdatePlaces(places);
  if (placesResult.results.length > 0) {
    do_throw("Unexpected success.");
  }
  for (let place of placesResult.errors) {
    do_print("Checking '" + place.info.uri.spec + "'");
    do_check_eq(place.resultCode, Cr.NS_ERROR_INVALID_ARG);
    do_check_false(yield promiseIsURIVisited(place.info.uri));
  }
  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* test_duplicate_guid_errors() {
  // This test ensures that trying to add a visit, with a guid already found in
  // another visit, fails.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_duplicate_guid_fails_first"),
    visits: [
      new VisitInfo(),
    ],
  };

  do_check_false(yield promiseIsURIVisited(place.uri));
  let placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  do_check_true(yield promiseIsURIVisited(placeInfo.uri));

  let badPlace = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_duplicate_guid_fails_second"),
    visits: [
      new VisitInfo(),
    ],
    guid: placeInfo.guid,
  };

  do_check_false(yield promiseIsURIVisited(badPlace.uri));
  placesResult = yield promiseUpdatePlaces(badPlace);
  if (placesResult.results.length > 0) {
    do_throw("Unexpected success.");
  }
  let badPlaceInfo = placesResult.errors[0];
  do_check_eq(badPlaceInfo.resultCode, Cr.NS_ERROR_STORAGE_CONSTRAINT);
  do_check_false(yield promiseIsURIVisited(badPlaceInfo.info.uri));

  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* test_invalid_referrerURI_ignored() {
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN +
                        "test_invalid_referrerURI_ignored"),
    visits: [
      new VisitInfo(),
    ],
  };
  place.visits[0].referrerURI = NetUtil.newURI(place.uri.spec + "_unvisistedURI");
  do_check_false(yield promiseIsURIVisited(place.uri));
  do_check_false(yield promiseIsURIVisited(place.visits[0].referrerURI));

  let placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  do_check_true(yield promiseIsURIVisited(placeInfo.uri));

  // Check to make sure we do not visit the invalid referrer.
  do_check_false(yield promiseIsURIVisited(place.visits[0].referrerURI));

  // Check to make sure from_visit is zero in database.
  let stmt = DBConn().createStatement(
    `SELECT from_visit
     FROM moz_historyvisits
     WHERE id = :visit_id`
  );
  stmt.params.visit_id = placeInfo.visits[0].visitId;
  do_check_true(stmt.executeStep());
  do_check_eq(stmt.row.from_visit, 0);
  stmt.finalize();

  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* test_nonnsIURI_referrerURI_ignored() {
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN +
                        "test_nonnsIURI_referrerURI_ignored"),
    visits: [
      new VisitInfo(),
    ],
  };
  place.visits[0].referrerURI = place.uri.spec + "_nonnsIURI";
  do_check_false(yield promiseIsURIVisited(place.uri));

  let placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  do_check_true(yield promiseIsURIVisited(placeInfo.uri));

  // Check to make sure from_visit is zero in database.
  let stmt = DBConn().createStatement(
    `SELECT from_visit
     FROM moz_historyvisits
     WHERE id = :visit_id`
  );
  stmt.params.visit_id = placeInfo.visits[0].visitId;
  do_check_true(stmt.executeStep());
  do_check_eq(stmt.row.from_visit, 0);
  stmt.finalize();

  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* test_old_referrer_ignored() {
  // This tests that a referrer for a visit which is not recent (specifically,
  // older than 15 minutes as per RECENT_EVENT_THRESHOLD) is not saved by
  // updatePlaces.
  let oldTime = (Date.now() * 1000) - (RECENT_EVENT_THRESHOLD + 1);
  let referrerPlace = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_old_referrer_ignored_referrer"),
    visits: [
      new VisitInfo(TRANSITION_LINK, oldTime),
    ],
  };

  // First we must add our referrer to the history so that it is not ignored
  // as being invalid.
  do_check_false(yield promiseIsURIVisited(referrerPlace.uri));
  let placesResult = yield promiseUpdatePlaces(referrerPlace);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }

  // Now that the referrer is added, we can add a page with a valid
  // referrer to determine if the recency of the referrer is taken into
  // account.
  do_check_true(yield promiseIsURIVisited(referrerPlace.uri));

  let visitInfo = new VisitInfo();
  visitInfo.referrerURI = referrerPlace.uri;
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_old_referrer_ignored_page"),
    visits: [
      visitInfo,
    ],
  };

  do_check_false(yield promiseIsURIVisited(place.uri));
  placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  do_check_true(yield promiseIsURIVisited(place.uri));

  // Though the visit will not contain the referrer, we must examine the
  // database to be sure.
  do_check_eq(placeInfo.visits[0].referrerURI, null);
  let stmt = DBConn().createStatement(
    `SELECT COUNT(1) AS count
     FROM moz_historyvisits
     WHERE place_id = (SELECT id FROM moz_places WHERE url = :page_url)
     AND from_visit = 0`
  );
  stmt.params.page_url = place.uri.spec;
  do_check_true(stmt.executeStep());
  do_check_eq(stmt.row.count, 1);
  stmt.finalize();

  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* test_place_id_ignored() {
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_place_id_ignored_first"),
    visits: [
      new VisitInfo(),
    ],
  };

  do_check_false(yield promiseIsURIVisited(place.uri));
  let placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  do_check_true(yield promiseIsURIVisited(place.uri));

  let placeId = placeInfo.placeId;
  do_check_neq(placeId, 0);

  let badPlace = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_place_id_ignored_second"),
    visits: [
      new VisitInfo(),
    ],
    placeId: placeId,
  };

  do_check_false(yield promiseIsURIVisited(badPlace.uri));
  placesResult = yield promiseUpdatePlaces(badPlace);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  placeInfo = placesResult.results[0];

  do_check_neq(placeInfo.placeId, placeId);
  do_check_true(yield promiseIsURIVisited(badPlace.uri));

  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* test_handleCompletion_called_when_complete() {
  // We test a normal visit, and embeded visit, and a uri that would fail
  // the canAddURI test to make sure that the notification happens after *all*
  // of them have had a callback.
  let places = [
    { uri: NetUtil.newURI(TEST_DOMAIN +
                          "test_handleCompletion_called_when_complete"),
      visits: [
        new VisitInfo(),
        new VisitInfo(TRANSITION_EMBED),
      ],
    },
    { uri: NetUtil.newURI("data:,Hello%2C%20World!"),
      visits: [
        new VisitInfo(),
      ],
    },
  ];
  do_check_false(yield promiseIsURIVisited(places[0].uri));
  do_check_false(yield promiseIsURIVisited(places[1].uri));

  const EXPECTED_COUNT_SUCCESS = 2;
  const EXPECTED_COUNT_FAILURE = 1;
  let callbackCountSuccess = 0;
  let callbackCountFailure = 0;

  let placesResult = yield promiseUpdatePlaces(places);
  for (let place of placesResult.results) {
    let checker = PlacesUtils.history.canAddURI(place.uri) ?
      do_check_true : do_check_false;
    callbackCountSuccess++;
  }
  for (let error of placesResult.errors) {
    callbackCountFailure++;
  }

  do_check_eq(callbackCountSuccess, EXPECTED_COUNT_SUCCESS);
  do_check_eq(callbackCountFailure, EXPECTED_COUNT_FAILURE);
  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* test_add_visit() {
  const VISIT_TIME = Date.now() * 1000;
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_add_visit"),
    title: "test_add_visit title",
    visits: [],
  };
  for (let t in PlacesUtils.history.TRANSITIONS) {
    let transitionType = PlacesUtils.history.TRANSITIONS[t];
    place.visits.push(new VisitInfo(transitionType, VISIT_TIME));
  }
  do_check_false(yield promiseIsURIVisited(place.uri));

  let callbackCount = 0;
  let placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  for (let placeInfo of placesResult.results) {
    do_check_true(yield promiseIsURIVisited(place.uri));

    // Check mozIPlaceInfo properties.
    do_check_true(place.uri.equals(placeInfo.uri));
    do_check_eq(placeInfo.frecency, -1); // We don't pass frecency here!
    do_check_eq(placeInfo.title, place.title);

    // Check mozIVisitInfo properties.
    let visits = placeInfo.visits;
    do_check_eq(visits.length, 1);
    let visit = visits[0];
    do_check_eq(visit.visitDate, VISIT_TIME);
    let transitions =
    do_check_true(Object.values(PlacesUtils.history.TRANSITIONS).includes(visit.transitionType));
    do_check_true(visit.referrerURI === null);

    // For TRANSITION_EMBED visits, many properties will always be zero or
    // undefined.
    if (visit.transitionType == TRANSITION_EMBED) {
      // Check mozIPlaceInfo properties.
      do_check_eq(placeInfo.placeId, 0, '//');
      do_check_eq(placeInfo.guid, null);

      // Check mozIVisitInfo properties.
      do_check_eq(visit.visitId, 0);
    }
    // But they should be valid for non-embed visits.
    else {
      // Check mozIPlaceInfo properties.
      do_check_true(placeInfo.placeId > 0);
      do_check_valid_places_guid(placeInfo.guid);

      // Check mozIVisitInfo properties.
      do_check_true(visit.visitId > 0);
    }

    // If we have had all of our callbacks, continue running tests.
    if (++callbackCount == place.visits.length) {
      yield PlacesTestUtils.promiseAsyncUpdates();
    }
  }
});

add_task(function* test_properties_saved() {
  // Check each transition type to make sure it is saved properly.
  let places = [];
  for (let t in PlacesUtils.history.TRANSITIONS) {
    let transitionType = PlacesUtils.history.TRANSITIONS[t];
    let place = {
      uri: NetUtil.newURI(TEST_DOMAIN + "test_properties_saved/" +
                          transitionType),
      title: "test_properties_saved test",
      visits: [
        new VisitInfo(transitionType),
      ],
    };
    do_check_false(yield promiseIsURIVisited(place.uri));
    places.push(place);
  }

  let callbackCount = 0;
  let placesResult = yield promiseUpdatePlaces(places);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  for (let placeInfo of placesResult.results) {
    let uri = placeInfo.uri;
    do_check_true(yield promiseIsURIVisited(uri));
    let visit = placeInfo.visits[0];
    print("TEST-INFO | test_properties_saved | updatePlaces callback for " +
          "transition type " + visit.transitionType);

    // Note that TRANSITION_EMBED should not be in the database.
    const EXPECTED_COUNT = visit.transitionType == TRANSITION_EMBED ? 0 : 1;

    // mozIVisitInfo::date
    let stmt = DBConn().createStatement(
      `SELECT COUNT(1) AS count
       FROM moz_places h
       JOIN moz_historyvisits v
       ON h.id = v.place_id
       WHERE h.url = :page_url
       AND v.visit_date = :visit_date`
    );
    stmt.params.page_url = uri.spec;
    stmt.params.visit_date = visit.visitDate;
    do_check_true(stmt.executeStep());
    do_check_eq(stmt.row.count, EXPECTED_COUNT);
    stmt.finalize();

    // mozIVisitInfo::transitionType
    stmt = DBConn().createStatement(
      `SELECT COUNT(1) AS count
       FROM moz_places h
       JOIN moz_historyvisits v
       ON h.id = v.place_id
       WHERE h.url = :page_url
       AND v.visit_type = :transition_type`
    );
    stmt.params.page_url = uri.spec;
    stmt.params.transition_type = visit.transitionType;
    do_check_true(stmt.executeStep());
    do_check_eq(stmt.row.count, EXPECTED_COUNT);
    stmt.finalize();

    // mozIPlaceInfo::title
    stmt = DBConn().createStatement(
      `SELECT COUNT(1) AS count
       FROM moz_places h
       WHERE h.url = :page_url
       AND h.title = :title`
    );
    stmt.params.page_url = uri.spec;
    stmt.params.title = placeInfo.title;
    do_check_true(stmt.executeStep());
    do_check_eq(stmt.row.count, EXPECTED_COUNT);
    stmt.finalize();

    // If we have had all of our callbacks, continue running tests.
    if (++callbackCount == places.length) {
      yield PlacesTestUtils.promiseAsyncUpdates();
    }
  }
});

add_task(function* test_guid_saved() {
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_guid_saved"),
    guid: "__TESTGUID__",
    visits: [
      new VisitInfo(),
    ],
  };
  do_check_valid_places_guid(place.guid);
  do_check_false(yield promiseIsURIVisited(place.uri));

  let placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  let uri = placeInfo.uri;
  do_check_true(yield promiseIsURIVisited(uri));
  do_check_eq(placeInfo.guid, place.guid);
  do_check_guid_for_uri(uri, place.guid);
  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* test_referrer_saved() {
  let places = [
    { uri: NetUtil.newURI(TEST_DOMAIN + "test_referrer_saved/referrer"),
      visits: [
        new VisitInfo(),
      ],
    },
    { uri: NetUtil.newURI(TEST_DOMAIN + "test_referrer_saved/test"),
      visits: [
        new VisitInfo(),
      ],
    },
  ];
  places[1].visits[0].referrerURI = places[0].uri;
  do_check_false(yield promiseIsURIVisited(places[0].uri));
  do_check_false(yield promiseIsURIVisited(places[1].uri));

  let resultCount = 0;
  let placesResult = yield promiseUpdatePlaces(places);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  for (let placeInfo of placesResult.results) {
    let uri = placeInfo.uri;
    do_check_true(yield promiseIsURIVisited(uri));
    let visit = placeInfo.visits[0];

    // We need to insert all of our visits before we can test conditions.
    if (++resultCount == places.length) {
      do_check_true(places[0].uri.equals(visit.referrerURI));

      let stmt = DBConn().createStatement(
        `SELECT COUNT(1) AS count
         FROM moz_historyvisits
         WHERE place_id = (SELECT id FROM moz_places WHERE url = :page_url)
         AND from_visit = (
           SELECT id
           FROM moz_historyvisits
           WHERE place_id = (SELECT id FROM moz_places WHERE url = :referrer)
         )`
      );
      stmt.params.page_url = uri.spec;
      stmt.params.referrer = visit.referrerURI.spec;
      do_check_true(stmt.executeStep());
      do_check_eq(stmt.row.count, 1);
      stmt.finalize();

      yield PlacesTestUtils.promiseAsyncUpdates();
    }
  }
});

add_task(function* test_guid_change_saved() {
  // First, add a visit for it.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_guid_change_saved"),
    visits: [
      new VisitInfo(),
    ],
  };
  do_check_false(yield promiseIsURIVisited(place.uri));

  let placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  // Then, change the guid with visits.
  place.guid = "_GUIDCHANGE_";
  place.visits = [new VisitInfo()];
  placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  do_check_guid_for_uri(place.uri, place.guid);

  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* test_title_change_saved() {
  // First, add a visit for it.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_title_change_saved"),
    title: "original title",
    visits: [
      new VisitInfo(),
    ],
  };
  do_check_false(yield promiseIsURIVisited(place.uri));

  let placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }

  // Now, make sure the empty string clears the title.
  place.title = "";
  place.visits = [new VisitInfo()];
  placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  do_check_title_for_uri(place.uri, null);

  // Then, change the title with visits.
  place.title = "title change";
  place.visits = [new VisitInfo()];
  placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  do_check_title_for_uri(place.uri, place.title);

  // Lastly, check that the title is cleared if we set it to null.
  place.title = null;
  place.visits = [new VisitInfo()];
  placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  do_check_title_for_uri(place.uri, place.title);

  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* test_no_title_does_not_clear_title() {
  const TITLE = "test title";
  // First, add a visit for it.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_no_title_does_not_clear_title"),
    title: TITLE,
    visits: [
      new VisitInfo(),
    ],
  };
  do_check_false(yield promiseIsURIVisited(place.uri));

  let placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  // Now, make sure that not specifying a title does not clear it.
  delete place.title;
  place.visits = [new VisitInfo()];
  placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }
  do_check_title_for_uri(place.uri, TITLE);

  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* test_title_change_notifies() {
  // There are three cases to test.  The first case is to make sure we do not
  // get notified if we do not specify a title.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_title_change_notifies"),
    visits: [
      new VisitInfo(),
    ],
  };
  do_check_false(yield promiseIsURIVisited(place.uri));

  let silentObserver =
    new TitleChangedObserver(place.uri, "DO NOT WANT", function() {
      do_throw("unexpected callback!");
    });

  PlacesUtils.history.addObserver(silentObserver, false);
  let placesResult = yield promiseUpdatePlaces(place);
  if (placesResult.errors.length > 0) {
    do_throw("Unexpected error.");
  }

  // The second case to test is that we get the notification when we add
  // it for the first time.  The first case will fail before our callback if it
  // is busted, so we can do this now.
  place.uri = NetUtil.newURI(place.uri.spec + "/new-visit-with-title");
  place.title = "title 1";
  function promiseTitleChangedObserver(aPlace) {
    return new Promise((resolve, reject) => {
      let callbackCount = 0;
      let observer = new TitleChangedObserver(aPlace.uri, aPlace.title, function() {
        switch (++callbackCount) {
          case 1:
            // The third case to test is to make sure we get a notification when
            // we change an existing place.
            observer.expectedTitle = place.title = "title 2";
            place.visits = [new VisitInfo()];
            PlacesUtils.asyncHistory.updatePlaces(place);
            break;
          case 2:
            PlacesUtils.history.removeObserver(silentObserver);
            PlacesUtils.history.removeObserver(observer);
            resolve();
            break;
        }
      });

      PlacesUtils.history.addObserver(observer, false);
      PlacesUtils.asyncHistory.updatePlaces(aPlace);
    });
  }

  yield promiseTitleChangedObserver(place);
  yield PlacesTestUtils.promiseAsyncUpdates();
});

add_task(function* test_visit_notifies() {
  // There are two observers we need to see for each visit.  One is an
  // nsINavHistoryObserver and the other is the uri-visit-saved observer topic.
  let place = {
    guid: "abcdefghijkl",
    uri: NetUtil.newURI(TEST_DOMAIN + "test_visit_notifies"),
    visits: [
      new VisitInfo(),
    ],
  };
  do_check_false(yield promiseIsURIVisited(place.uri));

  function promiseVisitObserver(aPlace) {
    return new Promise((resolve, reject) => {
      let callbackCount = 0;
      let finisher = function() {
        if (++callbackCount == 2) {
          resolve();
        }
      }
      let visitObserver = new VisitObserver(place.uri, place.guid,
                                            function(aVisitDate,
                                                     aTransitionType) {
        let visit = place.visits[0];
        do_check_eq(visit.visitDate, aVisitDate);
        do_check_eq(visit.transitionType, aTransitionType);

        PlacesUtils.history.removeObserver(visitObserver);
        finisher();
      });
      PlacesUtils.history.addObserver(visitObserver, false);
      let observer = function(aSubject, aTopic, aData) {
        do_print("observe(" + aSubject + ", " + aTopic + ", " + aData + ")");
        do_check_true(aSubject instanceof Ci.nsIURI);
        do_check_true(aSubject.equals(place.uri));

        Services.obs.removeObserver(observer, URI_VISIT_SAVED);
        finisher();
      };
      Services.obs.addObserver(observer, URI_VISIT_SAVED, false);
      PlacesUtils.asyncHistory.updatePlaces(place);
    });
  }

  yield promiseVisitObserver(place);
  yield PlacesTestUtils.promiseAsyncUpdates();
});

// test with empty mozIVisitInfoCallback object
add_task(function* test_callbacks_not_supplied() {
  const URLS = [
    "imap://cyrus.andrew.cmu.edu/archive.imap",  // bad URI
    "http://mozilla.org/" // valid URI
  ];
  let places = [];
  URLS.forEach(function(url) {
    try {
      let place = {
        uri: NetUtil.newURI(url),
        title: "test for " + url,
        visits: [
          new VisitInfo(),
        ],
      };
      places.push(place);
    }
    catch (e) {
      if (e.result != Cr.NS_ERROR_FAILURE) {
        throw e;
      }
      // NetUtil.newURI() can throw if e.g. our app knows about imap://
      // but the account is not set up and so the URL is invalid for us.
      // Note this in the log but ignore as it's not the subject of this test.
      do_print("Could not construct URI for '" + url + "'; ignoring");
    }
  });

  PlacesUtils.asyncHistory.updatePlaces(places, {});
  yield PlacesTestUtils.promiseAsyncUpdates();
});

// Test that we don't wrongly overwrite typed and hidden when adding new visits.
add_task(function* test_typed_hidden_not_overwritten() {
  yield PlacesTestUtils.clearHistory();
  let places = [
    { uri: NetUtil.newURI("http://mozilla.org/"),
      title: "test",
      visits: [
        new VisitInfo(TRANSITION_TYPED),
        new VisitInfo(TRANSITION_LINK)
      ]
    },
    { uri: NetUtil.newURI("http://mozilla.org/"),
      title: "test",
      visits: [
        new VisitInfo(TRANSITION_FRAMED_LINK)
      ]
    },
  ];
  yield promiseUpdatePlaces(places);

  let db = yield PlacesUtils.promiseDBConnection();
  let rows = yield db.execute("SELECT hidden, typed FROM moz_places WHERE url = :url",
                              { url: "http://mozilla.org/" });
  Assert.equal(rows[0].getResultByName("typed"), 1,
               "The page should be marked as typed");
  Assert.equal(rows[0].getResultByName("hidden"), 0,
               "The page should be marked as not hidden");
});

function run_test()
{
  run_next_test();
}

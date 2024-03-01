/**
 * This file tests the async history API exposed by mozIAsyncHistory.
 */

// Globals

XPCOMUtils.defineLazyServiceGetter(
  this,
  "asyncHistory",
  "@mozilla.org/browser/history;1",
  "mozIAsyncHistory"
);

const TEST_DOMAIN = "http://mozilla.org/";
const URI_VISIT_SAVED = "uri-visit-saved";
const RECENT_EVENT_THRESHOLD = 15 * 60 * 1000000;

// Helpers
/**
 * Object that represents a mozIVisitInfo object.
 *
 * @param [optional] aTransitionType
 *        The transition type of the visit.  Defaults to TRANSITION_LINK if not
 *        provided.
 * @param [optional] aVisitTime
 *        The time of the visit.  Defaults to now if not provided.
 */
function VisitInfo(aTransitionType, aVisitTime) {
  this.transitionType =
    aTransitionType === undefined ? TRANSITION_LINK : aTransitionType;
  this.visitDate = aVisitTime || Date.now() * 1000;
}

function promiseUpdatePlaces(aPlaces, aOptions = {}) {
  return new Promise(resolve => {
    asyncHistory.updatePlaces(
      aPlaces,
      Object.assign(
        {
          _errors: [],
          _results: [],
          handleError(aResultCode, aPlace) {
            this._errors.push({ resultCode: aResultCode, info: aPlace });
          },
          handleResult(aPlace) {
            this._results.push(aPlace);
          },
          handleCompletion(resultCount) {
            resolve({
              errors: this._errors,
              results: this._results,
              resultCount,
            });
          },
        },
        aOptions
      )
    );
  });
}

/**
 * Listens for a title change notification, and calls aCallback when it gets it.
 */
class TitleChangedObserver {
  /**
   * Constructor.
   *
   * @param aURI
   *        The URI of the page we expect a notification for.
   * @param aExpectedTitle
   *        The expected title of the URI we expect a notification for.
   * @param aCallback
   *        The method to call when we have gotten the proper notification about
   *        the title changing.
   */
  constructor(aURI, aExpectedTitle, aCallback) {
    this.uri = aURI;
    this.expectedTitle = aExpectedTitle;
    this.callback = aCallback;
    this.handlePlacesEvent = this.handlePlacesEvent.bind(this);
    PlacesObservers.addListener(["page-title-changed"], this.handlePlacesEvent);
  }

  async handlePlacesEvent(aEvents) {
    info("'page-title-changed'!!!");
    Assert.equal(aEvents.length, 1, "Right number of title changed notified");
    Assert.equal(aEvents[0].type, "page-title-changed");
    if (this.uri.spec !== aEvents[0].url) {
      return;
    }
    Assert.equal(aEvents[0].title, this.expectedTitle);
    await check_guid_for_uri(this.uri, aEvents[0].pageGuid);
    this.callback();

    PlacesObservers.removeListener(
      ["page-title-changed"],
      this.handlePlacesEvent
    );
  }
}

/**
 * Listens for a visit notification, and calls aCallback when it gets it.
 */
class VisitObserver {
  constructor(aURI, aGUID, aCallback) {
    this.uri = aURI;
    this.guid = aGUID;
    this.callback = aCallback;
    this.handlePlacesEvent = this.handlePlacesEvent.bind(this);
    PlacesObservers.addListener(["page-visited"], this.handlePlacesEvent);
  }

  handlePlacesEvent(aEvents) {
    info("'page-visited'!!!");
    Assert.equal(aEvents.length, 1, "Right number of visits notified");
    Assert.equal(aEvents[0].type, "page-visited");
    let {
      url,
      visitId,
      visitTime,
      referringVisitId,
      transitionType,
      pageGuid,
      hidden,
      visitCount,
      typedCount,
      lastKnownTitle,
    } = aEvents[0];
    let args = [
      visitId,
      visitTime,
      referringVisitId,
      transitionType,
      pageGuid,
      hidden,
      visitCount,
      typedCount,
      lastKnownTitle,
    ];
    info("'page-visited' (" + url + args.join(", ") + ")");
    if (this.uri.spec != url || this.guid != pageGuid) {
      return;
    }
    this.callback(visitTime * 1000, transitionType, lastKnownTitle);

    PlacesObservers.removeListener(["page-visited"], this.handlePlacesEvent);
  }
}

/**
 * Tests that a title was set properly in the database.
 *
 * @param aURI
 *        The uri to check.
 * @param aTitle
 *        The expected title in the database.
 */
function do_check_title_for_uri(aURI, aTitle) {
  let stmt = DBConn().createStatement(
    `SELECT title
     FROM moz_places
     WHERE url_hash = hash(:url) AND url = :url`
  );
  stmt.params.url = aURI.spec;
  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.row.title, aTitle);
  stmt.finalize();
}

// Test Functions

add_task(async function test_interface_exists() {
  let history = Cc["@mozilla.org/browser/history;1"].getService(Ci.nsISupports);
  Assert.ok(history instanceof Ci.mozIAsyncHistory);
});

add_task(async function test_invalid_uri_throws() {
  // First, test passing in nothing.
  let place = {
    visits: [new VisitInfo()],
  };
  try {
    await promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_INVALID_ARG);
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
      await promiseUpdatePlaces(place);
      do_throw("Should have thrown!");
    } catch (e) {
      Assert.equal(e.result, Cr.NS_ERROR_INVALID_ARG);
    }
  }
});

add_task(async function test_invalid_places_throws() {
  // First, test passing in nothing.
  try {
    asyncHistory.updatePlaces();
    do_throw("Should have thrown!");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_XPC_NOT_ENOUGH_ARGS);
  }

  // Now, test other bogus things.
  const TEST_VALUES = [null, undefined, {}, [], ""];
  for (let i = 0; i < TEST_VALUES.length; i++) {
    let value = TEST_VALUES[i];
    try {
      await promiseUpdatePlaces(value);
      do_throw("Should have thrown!");
    } catch (e) {
      Assert.equal(e.result, Cr.NS_ERROR_INVALID_ARG);
    }
  }
});

add_task(async function test_invalid_guid_throws() {
  // First check invalid length guid.
  let place = {
    guid: "BAD_GUID",
    uri: NetUtil.newURI(TEST_DOMAIN + "test_invalid_guid_throws"),
    visits: [new VisitInfo()],
  };
  try {
    await promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_INVALID_ARG);
  }

  // Now check invalid character guid.
  place.guid = "__BADGUID+__";
  Assert.equal(place.guid.length, 12);
  try {
    await promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(async function test_no_visits_throws() {
  const TEST_URI = NetUtil.newURI(
    TEST_DOMAIN + "test_no_id_or_guid_no_visits_throws"
  );
  const TEST_GUID = "_RANDOMGUID_";

  let log_test_conditions = function (aPlace) {
    let str =
      "Testing place with " +
      (aPlace.uri ? "uri" : "no uri") +
      ", " +
      (aPlace.guid ? "guid" : "no guid") +
      ", " +
      (aPlace.visits ? "visits array" : "no visits array");
    info(str);
  };

  // Loop through every possible case.  Note that we don't actually care about
  // the case where we have no uri, place id, or guid (covered by another test),
  // but it is easier to just make sure it too throws than to exclude it.
  let place = {};
  for (let uri = 1; uri >= 0; uri--) {
    place.uri = uri ? TEST_URI : undefined;

    for (let guid = 1; guid >= 0; guid--) {
      place.guid = guid ? TEST_GUID : undefined;

      for (let visits = 1; visits >= 0; visits--) {
        place.visits = visits ? [] : undefined;

        log_test_conditions(place);
        try {
          await promiseUpdatePlaces(place);
          do_throw("Should have thrown!");
        } catch (e) {
          Assert.equal(e.result, Cr.NS_ERROR_INVALID_ARG);
        }
      }
    }
  }
});

add_task(async function test_add_visit_no_date_throws() {
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_add_visit_no_date_throws"),
    visits: [new VisitInfo()],
  };
  delete place.visits[0].visitDate;
  try {
    await promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(async function test_add_visit_no_transitionType_throws() {
  let place = {
    uri: NetUtil.newURI(
      TEST_DOMAIN + "test_add_visit_no_transitionType_throws"
    ),
    visits: [new VisitInfo()],
  };
  delete place.visits[0].transitionType;
  try {
    await promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(async function test_add_visit_invalid_transitionType_throws() {
  // First, test something that has a transition type lower than the first one.
  let place = {
    uri: NetUtil.newURI(
      TEST_DOMAIN + "test_add_visit_invalid_transitionType_throws"
    ),
    visits: [new VisitInfo(TRANSITION_LINK - 1)],
  };
  try {
    await promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_INVALID_ARG);
  }

  // Now, test something that has a transition type greater than the last one.
  place.visits[0] = new VisitInfo(TRANSITION_RELOAD + 1);
  try {
    await promiseUpdatePlaces(place);
    do_throw("Should have thrown!");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_INVALID_ARG);
  }
});

add_task(async function test_non_addable_uri_errors() {
  // Array of protocols that nsINavHistoryService::canAddURI returns false for.
  const URLS = [
    "about:config",
    "imap://cyrus.andrew.cmu.edu/archive.imap",
    "news://new.mozilla.org/mozilla.dev.apps.firefox",
    "mailbox:Inbox",
    "cached-favicon:http://mozilla.org/made-up-favicon",
    "view-source:http://mozilla.org",
    "chrome://browser/content/browser.xhtml",
    "resource://gre-resources/hiddenWindow.html",
    "data:,Hello%2C%20World!",
    "javascript:alert('hello wolrd!');",
    "blob:foo",
    "moz-extension://f49fb5b3-a1e7-cd41-85e1-d61a3950f5e4/index.html",
  ];
  let places = [];
  URLS.forEach(function (url) {
    try {
      let place = {
        uri: NetUtil.newURI(url),
        title: "test for " + url,
        visits: [new VisitInfo()],
      };
      places.push(place);
    } catch (e) {
      if (e.result != Cr.NS_ERROR_FAILURE) {
        throw e;
      }
      // NetUtil.newURI() can throw if e.g. our app knows about imap://
      // but the account is not set up and so the URL is invalid for us.
      // Note this in the log but ignore as it's not the subject of this test.
      info("Could not construct URI for '" + url + "'; ignoring");
    }
  });

  let placesResult = await promiseUpdatePlaces(places);
  if (placesResult.results.length) {
    do_throw("Unexpected success.");
  }
  for (let place of placesResult.errors) {
    info("Checking '" + place.info.uri.spec + "'");
    Assert.equal(place.resultCode, Cr.NS_ERROR_INVALID_ARG);
    Assert.equal(false, await PlacesUtils.history.hasVisits(place.info.uri));
  }
  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_duplicate_guid_errors() {
  // This test ensures that trying to add a visit, with a guid already found in
  // another visit, fails.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_duplicate_guid_fails_first"),
    visits: [new VisitInfo()],
  };

  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));
  let placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  Assert.ok(await PlacesUtils.history.hasVisits(placeInfo.uri));

  let badPlace = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_duplicate_guid_fails_second"),
    visits: [new VisitInfo()],
    guid: placeInfo.guid,
  };

  Assert.equal(false, await PlacesUtils.history.hasVisits(badPlace.uri));
  placesResult = await promiseUpdatePlaces(badPlace);
  if (placesResult.results.length) {
    do_throw("Unexpected success.");
  }
  let badPlaceInfo = placesResult.errors[0];
  Assert.equal(badPlaceInfo.resultCode, Cr.NS_ERROR_STORAGE_CONSTRAINT);
  Assert.equal(
    false,
    await PlacesUtils.history.hasVisits(badPlaceInfo.info.uri)
  );

  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_invalid_referrerURI_ignored() {
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_invalid_referrerURI_ignored"),
    visits: [new VisitInfo()],
  };
  place.visits[0].referrerURI = NetUtil.newURI(
    place.uri.spec + "_unvisistedURI"
  );
  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));
  Assert.equal(
    false,
    await PlacesUtils.history.hasVisits(place.visits[0].referrerURI)
  );

  let placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  Assert.ok(await PlacesUtils.history.hasVisits(placeInfo.uri));

  // Check to make sure we do not visit the invalid referrer.
  Assert.equal(
    false,
    await PlacesUtils.history.hasVisits(place.visits[0].referrerURI)
  );

  // Check to make sure from_visit is zero in database.
  let stmt = DBConn().createStatement(
    `SELECT from_visit
     FROM moz_historyvisits
     WHERE id = :visit_id`
  );
  stmt.params.visit_id = placeInfo.visits[0].visitId;
  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.row.from_visit, 0);
  stmt.finalize();

  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_nonnsIURI_referrerURI_ignored() {
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_nonnsIURI_referrerURI_ignored"),
    visits: [new VisitInfo()],
  };
  place.visits[0].referrerURI = place.uri.spec + "_nonnsIURI";
  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));

  let placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  Assert.ok(await PlacesUtils.history.hasVisits(placeInfo.uri));

  // Check to make sure from_visit is zero in database.
  let stmt = DBConn().createStatement(
    `SELECT from_visit
     FROM moz_historyvisits
     WHERE id = :visit_id`
  );
  stmt.params.visit_id = placeInfo.visits[0].visitId;
  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.row.from_visit, 0);
  stmt.finalize();

  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_old_referrer_ignored() {
  // This tests that a referrer for a visit which is not recent (specifically,
  // older than 15 minutes as per RECENT_EVENT_THRESHOLD) is not saved by
  // updatePlaces.
  let oldTime = Date.now() * 1000 - (RECENT_EVENT_THRESHOLD + 1);
  let referrerPlace = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_old_referrer_ignored_referrer"),
    visits: [new VisitInfo(TRANSITION_LINK, oldTime)],
  };

  // First we must add our referrer to the history so that it is not ignored
  // as being invalid.
  Assert.equal(false, await PlacesUtils.history.hasVisits(referrerPlace.uri));
  let placesResult = await promiseUpdatePlaces(referrerPlace);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }

  // Now that the referrer is added, we can add a page with a valid
  // referrer to determine if the recency of the referrer is taken into
  // account.
  Assert.ok(await PlacesUtils.history.hasVisits(referrerPlace.uri));

  let visitInfo = new VisitInfo();
  visitInfo.referrerURI = referrerPlace.uri;
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_old_referrer_ignored_page"),
    visits: [visitInfo],
  };

  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));
  placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  Assert.ok(await PlacesUtils.history.hasVisits(place.uri));

  // Though the visit will not contain the referrer, we must examine the
  // database to be sure.
  Assert.equal(placeInfo.visits[0].referrerURI, null);
  let stmt = DBConn().createStatement(
    `SELECT COUNT(1) AS count
     FROM moz_historyvisits
     JOIN moz_places h ON h.id = place_id
     WHERE url_hash = hash(:page_url) AND url = :page_url
     AND from_visit = 0`
  );
  stmt.params.page_url = place.uri.spec;
  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.row.count, 1);
  stmt.finalize();

  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_place_id_ignored() {
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_place_id_ignored_first"),
    visits: [new VisitInfo()],
  };

  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));
  let placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  Assert.ok(await PlacesUtils.history.hasVisits(place.uri));

  let placeId = placeInfo.placeId;
  Assert.notEqual(placeId, 0);

  let badPlace = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_place_id_ignored_second"),
    visits: [new VisitInfo()],
    placeId,
  };

  Assert.equal(false, await PlacesUtils.history.hasVisits(badPlace.uri));
  placesResult = await promiseUpdatePlaces(badPlace);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  placeInfo = placesResult.results[0];

  Assert.notEqual(placeInfo.placeId, placeId);
  Assert.ok(await PlacesUtils.history.hasVisits(badPlace.uri));

  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_handleCompletion_called_when_complete() {
  // We test a normal visit, and embeded visit, and a uri that would fail
  // the canAddURI test to make sure that the notification happens after *all*
  // of them have had a callback.
  let places = [
    {
      uri: NetUtil.newURI(
        TEST_DOMAIN + "test_handleCompletion_called_when_complete"
      ),
      visits: [new VisitInfo(), new VisitInfo(TRANSITION_EMBED)],
    },
    {
      uri: NetUtil.newURI("data:,Hello%2C%20World!"),
      visits: [new VisitInfo()],
    },
  ];
  Assert.equal(false, await PlacesUtils.history.hasVisits(places[0].uri));
  Assert.equal(false, await PlacesUtils.history.hasVisits(places[1].uri));

  const EXPECTED_COUNT_SUCCESS = 2;
  const EXPECTED_COUNT_FAILURE = 1;

  let { results, errors } = await promiseUpdatePlaces(places);

  Assert.equal(results.length, EXPECTED_COUNT_SUCCESS);
  Assert.equal(errors.length, EXPECTED_COUNT_FAILURE);

  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_add_visit() {
  const VISIT_TIME = Date.now() * 1000;
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_add_visit"),
    title: "test_add_visit title",
    visits: [],
  };
  for (let t in PlacesUtils.history.TRANSITIONS) {
    if (t == "EMBED") {
      continue;
    }
    let transitionType = PlacesUtils.history.TRANSITIONS[t];
    place.visits.push(new VisitInfo(transitionType, VISIT_TIME));
  }
  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));

  let callbackCount = 0;
  let placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  for (let placeInfo of placesResult.results) {
    Assert.ok(await PlacesUtils.history.hasVisits(place.uri));

    // Check mozIPlaceInfo properties.
    Assert.ok(place.uri.equals(placeInfo.uri));
    Assert.equal(placeInfo.frecency, -1); // We don't pass frecency here!
    Assert.equal(placeInfo.title, place.title);

    // Check mozIVisitInfo properties.
    let visits = placeInfo.visits;
    Assert.equal(visits.length, 1);
    let visit = visits[0];
    Assert.equal(visit.visitDate, VISIT_TIME);
    Assert.ok(
      Object.values(PlacesUtils.history.TRANSITIONS).includes(
        visit.transitionType
      )
    );
    Assert.ok(visit.referrerURI === null);

    // For TRANSITION_EMBED visits, many properties will always be zero or
    // undefined.
    if (visit.transitionType == TRANSITION_EMBED) {
      // Check mozIPlaceInfo properties.
      Assert.equal(placeInfo.placeId, 0, "//");
      Assert.equal(placeInfo.guid, null);

      // Check mozIVisitInfo properties.
      Assert.equal(visit.visitId, 0);
    } else {
      // But they should be valid for non-embed visits.
      // Check mozIPlaceInfo properties.
      Assert.ok(placeInfo.placeId > 0);
      do_check_valid_places_guid(placeInfo.guid);

      // Check mozIVisitInfo properties.
      Assert.ok(visit.visitId > 0);
    }

    // If we have had all of our callbacks, continue running tests.
    if (++callbackCount == place.visits.length) {
      await PlacesTestUtils.promiseAsyncUpdates();
    }
  }
});

add_task(async function test_properties_saved() {
  // Check each transition type to make sure it is saved properly.
  let places = [];
  for (let t in PlacesUtils.history.TRANSITIONS) {
    if (t == "EMBED") {
      continue;
    }
    let transitionType = PlacesUtils.history.TRANSITIONS[t];
    let place = {
      uri: NetUtil.newURI(
        TEST_DOMAIN + "test_properties_saved/" + transitionType
      ),
      title: "test_properties_saved test",
      visits: [new VisitInfo(transitionType)],
    };
    Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));
    places.push(place);
  }

  let callbackCount = 0;
  let placesResult = await promiseUpdatePlaces(places);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  for (let placeInfo of placesResult.results) {
    let uri = placeInfo.uri;
    Assert.ok(await PlacesUtils.history.hasVisits(uri));
    let visit = placeInfo.visits[0];
    print(
      "TEST-INFO | test_properties_saved | updatePlaces callback for " +
        "transition type " +
        visit.transitionType
    );

    // Note that TRANSITION_EMBED should not be in the database.
    const EXPECTED_COUNT = visit.transitionType == TRANSITION_EMBED ? 0 : 1;

    // mozIVisitInfo::date
    let stmt = DBConn().createStatement(
      `SELECT COUNT(1) AS count
       FROM moz_places h
       JOIN moz_historyvisits v
       ON h.id = v.place_id
       WHERE h.url_hash = hash(:page_url) AND h.url = :page_url
       AND v.visit_date = :visit_date`
    );
    stmt.params.page_url = uri.spec;
    stmt.params.visit_date = visit.visitDate;
    Assert.ok(stmt.executeStep());
    Assert.equal(stmt.row.count, EXPECTED_COUNT);
    stmt.finalize();

    // mozIVisitInfo::transitionType
    stmt = DBConn().createStatement(
      `SELECT COUNT(1) AS count
       FROM moz_places h
       JOIN moz_historyvisits v
       ON h.id = v.place_id
       WHERE h.url_hash = hash(:page_url) AND h.url = :page_url
       AND v.visit_type = :transition_type`
    );
    stmt.params.page_url = uri.spec;
    stmt.params.transition_type = visit.transitionType;
    Assert.ok(stmt.executeStep());
    Assert.equal(stmt.row.count, EXPECTED_COUNT);
    stmt.finalize();

    // mozIPlaceInfo::title
    stmt = DBConn().createStatement(
      `SELECT COUNT(1) AS count
       FROM moz_places h
       WHERE h.url_hash = hash(:page_url) AND h.url = :page_url
       AND h.title = :title`
    );
    stmt.params.page_url = uri.spec;
    stmt.params.title = placeInfo.title;
    Assert.ok(stmt.executeStep());
    Assert.equal(stmt.row.count, EXPECTED_COUNT);
    stmt.finalize();

    // If we have had all of our callbacks, continue running tests.
    if (++callbackCount == places.length) {
      await PlacesTestUtils.promiseAsyncUpdates();
    }
  }
});

add_task(async function test_guid_saved() {
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_guid_saved"),
    guid: "__TESTGUID__",
    visits: [new VisitInfo()],
  };
  do_check_valid_places_guid(place.guid);
  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));

  let placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  let uri = placeInfo.uri;
  Assert.ok(await PlacesUtils.history.hasVisits(uri));
  Assert.equal(placeInfo.guid, place.guid);
  await check_guid_for_uri(uri, place.guid);
  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_referrer_saved() {
  let places = [
    {
      uri: NetUtil.newURI(TEST_DOMAIN + "test_referrer_saved/referrer"),
      visits: [new VisitInfo()],
    },
    {
      uri: NetUtil.newURI(TEST_DOMAIN + "test_referrer_saved/test"),
      visits: [new VisitInfo()],
    },
  ];
  places[1].visits[0].referrerURI = places[0].uri;
  Assert.equal(false, await PlacesUtils.history.hasVisits(places[0].uri));
  Assert.equal(false, await PlacesUtils.history.hasVisits(places[1].uri));

  let resultCount = 0;
  let placesResult = await promiseUpdatePlaces(places);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  for (let placeInfo of placesResult.results) {
    let uri = placeInfo.uri;
    Assert.ok(await PlacesUtils.history.hasVisits(uri));
    let visit = placeInfo.visits[0];

    // We need to insert all of our visits before we can test conditions.
    if (++resultCount == places.length) {
      Assert.ok(places[0].uri.equals(visit.referrerURI));

      let stmt = DBConn().createStatement(
        `SELECT COUNT(1) AS count
         FROM moz_historyvisits
         JOIN moz_places h ON h.id = place_id
         WHERE url_hash = hash(:page_url) AND url = :page_url
         AND from_visit = (
           SELECT v.id
           FROM moz_historyvisits v
           JOIN moz_places h ON h.id = place_id
           WHERE url_hash = hash(:referrer) AND url = :referrer
         )`
      );
      stmt.params.page_url = uri.spec;
      stmt.params.referrer = visit.referrerURI.spec;
      Assert.ok(stmt.executeStep());
      Assert.equal(stmt.row.count, 1);
      stmt.finalize();

      await PlacesTestUtils.promiseAsyncUpdates();
    }
  }
});

add_task(async function test_guid_change_saved() {
  // First, add a visit for it.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_guid_change_saved"),
    visits: [new VisitInfo()],
  };
  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));

  let placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  // Then, change the guid with visits.
  place.guid = "_GUIDCHANGE_";
  place.visits = [new VisitInfo()];
  placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  await check_guid_for_uri(place.uri, place.guid);

  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_title_change_saved() {
  // First, add a visit for it.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_title_change_saved"),
    title: "original title",
    visits: [new VisitInfo()],
  };
  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));

  let placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }

  // Now, make sure the empty string clears the title.
  place.title = "";
  place.visits = [new VisitInfo()];
  placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  do_check_title_for_uri(place.uri, null);

  // Then, change the title with visits.
  place.title = "title change";
  place.visits = [new VisitInfo()];
  placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  do_check_title_for_uri(place.uri, place.title);

  // Lastly, check that the title is cleared if we set it to null.
  place.title = null;
  place.visits = [new VisitInfo()];
  placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  do_check_title_for_uri(place.uri, place.title);

  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_no_title_does_not_clear_title() {
  const TITLE = "test title";
  // First, add a visit for it.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_no_title_does_not_clear_title"),
    title: TITLE,
    visits: [new VisitInfo()],
  };
  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));

  let placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  // Now, make sure that not specifying a title does not clear it.
  delete place.title;
  place.visits = [new VisitInfo()];
  placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  do_check_title_for_uri(place.uri, TITLE);

  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_title_change_notifies() {
  // There are three cases to test.  The first case is to make sure we do not
  // get notified if we do not specify a title.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_title_change_notifies"),
    visits: [new VisitInfo()],
  };
  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));

  new TitleChangedObserver(place.uri, "DO NOT WANT", function () {
    do_throw("unexpected callback!");
  });

  let placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }

  // The second case to test is that we don't get the notification when we add
  // it for the first time.  The first case will fail before our callback if it
  // is busted, so we can do this now.
  place.uri = NetUtil.newURI(place.uri.spec + "/new-visit-with-title");
  place.title = "title 1";
  let expectedNotification = false;
  let titleChangeObserver;
  let titleChangePromise = new Promise(resolve => {
    titleChangeObserver = new TitleChangedObserver(
      place.uri,
      place.title,
      function () {
        Assert.ok(
          expectedNotification,
          "Should not get notified for " +
            place.uri.spec +
            " with title " +
            place.title
        );
        if (expectedNotification) {
          resolve();
        }
      }
    );
  });

  let visitPromise = new Promise(resolve => {
    function onVisits(events) {
      Assert.equal(events.length, 1, "Should only get notified for one visit.");
      Assert.equal(events[0].type, "page-visited");
      let { url } = events[0];
      Assert.equal(
        url,
        place.uri.spec,
        "Should get notified for visiting the new URI."
      );
      PlacesObservers.removeListener(["page-visited"], onVisits);
      resolve();
    }
    PlacesObservers.addListener(["page-visited"], onVisits);
  });
  asyncHistory.updatePlaces(place);
  await visitPromise;

  // The third case to test is to make sure we get a notification when
  // we change an existing place.
  expectedNotification = true;
  titleChangeObserver.expectedTitle = place.title = "title 2";
  place.visits = [new VisitInfo()];
  asyncHistory.updatePlaces(place);

  await titleChangePromise;
  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_visit_notifies() {
  // There are two observers we need to see for each visit. One is an
  // PlacesObservers and the other is the uri-visit-saved observer topic.
  let place = {
    guid: "abcdefghijkl",
    uri: NetUtil.newURI(TEST_DOMAIN + "test_visit_notifies"),
    visits: [new VisitInfo()],
  };
  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));

  function promiseVisitObserver() {
    return new Promise(resolve => {
      let callbackCount = 0;
      let finisher = function () {
        if (++callbackCount == 2) {
          resolve();
        }
      };
      new VisitObserver(place.uri, place.guid, function (
        aVisitDate,
        aTransitionType
      ) {
        let visit = place.visits[0];
        Assert.equal(visit.visitDate, aVisitDate);
        Assert.equal(visit.transitionType, aTransitionType);

        finisher();
      });
      let observer = function (aSubject, aTopic, aData) {
        info("observe(" + aSubject + ", " + aTopic + ", " + aData + ")");
        Assert.ok(aSubject instanceof Ci.nsIURI);
        Assert.ok(aSubject.equals(place.uri));

        Services.obs.removeObserver(observer, URI_VISIT_SAVED);
        finisher();
      };
      Services.obs.addObserver(observer, URI_VISIT_SAVED);
      asyncHistory.updatePlaces(place);
    });
  }

  await promiseVisitObserver(place);
  await PlacesTestUtils.promiseAsyncUpdates();
});

// test with empty mozIVisitInfoCallback object
add_task(async function test_callbacks_not_supplied() {
  const URLS = [
    "imap://cyrus.andrew.cmu.edu/archive.imap", // bad URI
    "http://mozilla.org/", // valid URI
  ];
  let places = [];
  URLS.forEach(function (url) {
    try {
      let place = {
        uri: NetUtil.newURI(url),
        title: "test for " + url,
        visits: [new VisitInfo()],
      };
      places.push(place);
    } catch (e) {
      if (e.result != Cr.NS_ERROR_FAILURE) {
        throw e;
      }
      // NetUtil.newURI() can throw if e.g. our app knows about imap://
      // but the account is not set up and so the URL is invalid for us.
      // Note this in the log but ignore as it's not the subject of this test.
      info("Could not construct URI for '" + url + "'; ignoring");
    }
  });

  asyncHistory.updatePlaces(places, {});
  await PlacesTestUtils.promiseAsyncUpdates();
});

// Test that we don't wrongly overwrite typed and hidden when adding new visits.
add_task(async function test_typed_hidden_not_overwritten() {
  await PlacesUtils.history.clear();
  let places = [
    {
      uri: NetUtil.newURI("http://mozilla.org/"),
      title: "test",
      visits: [new VisitInfo(TRANSITION_TYPED), new VisitInfo(TRANSITION_LINK)],
    },
    {
      uri: NetUtil.newURI("http://mozilla.org/"),
      title: "test",
      visits: [new VisitInfo(TRANSITION_FRAMED_LINK)],
    },
  ];
  await promiseUpdatePlaces(places);

  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(
    "SELECT hidden, typed FROM moz_places WHERE url_hash = hash(:url) AND url = :url",
    { url: "http://mozilla.org/" }
  );
  Assert.equal(
    rows[0].getResultByName("typed"),
    1,
    "The page should be marked as typed"
  );
  Assert.equal(
    rows[0].getResultByName("hidden"),
    0,
    "The page should be marked as not hidden"
  );
  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_omit_frecency_notifications() {
  // When multiple entries are inserted, frecency is calculated delayed, so
  // we won't get a ranking changed notification until recalculation happens.
  await PlacesUtils.history.clear();
  let notified = false;
  let listener = () => {
    notified = true;
    PlacesUtils.observers.removeListener(["pages-rank-changed"], listener);
  };
  PlacesUtils.observers.addListener(["pages-rank-changed"], listener);
  let places = [
    {
      uri: NetUtil.newURI("http://mozilla.org/"),
      title: "test",
      visits: [new VisitInfo(TRANSITION_TYPED)],
    },
    {
      uri: NetUtil.newURI("http://example.org/"),
      title: "test",
      visits: [new VisitInfo(TRANSITION_TYPED)],
    },
  ];
  await promiseUpdatePlaces(places);
  Assert.ok(!notified);
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  Assert.ok(notified);
});

add_task(async function test_ignore_errors() {
  await PlacesUtils.history.clear();
  // This test ensures that trying to add a visit, with a guid already found in
  // another visit, fails - but doesn't report if we told it not to.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_duplicate_guid_fails_first"),
    visits: [new VisitInfo()],
  };

  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));
  let placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  Assert.ok(await PlacesUtils.history.hasVisits(placeInfo.uri));

  let badPlace = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_duplicate_guid_fails_second"),
    visits: [new VisitInfo()],
    guid: placeInfo.guid,
  };

  Assert.equal(false, await PlacesUtils.history.hasVisits(badPlace.uri));
  placesResult = await promiseUpdatePlaces(badPlace, { ignoreErrors: true });
  if (placesResult.results.length) {
    do_throw("Unexpected success.");
  }
  Assert.equal(
    placesResult.errors.length,
    0,
    "Should have seen 0 errors because we disabled reporting."
  );
  Assert.equal(
    placesResult.results.length,
    0,
    "Should have seen 0 results because there were none."
  );
  Assert.equal(
    placesResult.resultCount,
    0,
    "Should know that we updated 0 items from the completion callback."
  );
  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_ignore_results() {
  await PlacesUtils.history.clear();
  let place = {
    uri: NetUtil.newURI("http://mozilla.org/"),
    title: "test",
    visits: [new VisitInfo()],
  };
  let placesResult = await promiseUpdatePlaces(place, { ignoreResults: true });
  Assert.equal(
    placesResult.results.length,
    0,
    "Should have seen 0 results because we disabled reporting."
  );
  Assert.equal(
    placesResult.errors.length,
    0,
    "Should have seen 0 errors because there were none."
  );
  Assert.equal(
    placesResult.resultCount,
    1,
    "Should know that we updated 1 item from the completion callback."
  );
  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_ignore_results_and_errors() {
  await PlacesUtils.history.clear();
  // This test ensures that trying to add a visit, with a guid already found in
  // another visit, fails - but doesn't report if we told it not to.
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_duplicate_guid_fails_first"),
    visits: [new VisitInfo()],
  };

  Assert.equal(false, await PlacesUtils.history.hasVisits(place.uri));
  let placesResult = await promiseUpdatePlaces(place);
  if (placesResult.errors.length) {
    do_throw("Unexpected error.");
  }
  let placeInfo = placesResult.results[0];
  Assert.ok(await PlacesUtils.history.hasVisits(placeInfo.uri));

  let badPlace = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_duplicate_guid_fails_second"),
    visits: [new VisitInfo()],
    guid: placeInfo.guid,
  };
  let allPlaces = [
    {
      uri: NetUtil.newURI(TEST_DOMAIN + "test_other_successful_item"),
      visits: [new VisitInfo()],
    },
    badPlace,
  ];

  Assert.equal(false, await PlacesUtils.history.hasVisits(badPlace.uri));
  placesResult = await promiseUpdatePlaces(allPlaces, {
    ignoreErrors: true,
    ignoreResults: true,
  });
  Assert.equal(
    placesResult.errors.length,
    0,
    "Should have seen 0 errors because we disabled reporting."
  );
  Assert.equal(
    placesResult.results.length,
    0,
    "Should have seen 0 results because we disabled reporting."
  );
  Assert.equal(
    placesResult.resultCount,
    1,
    "Should know that we updated 1 item from the completion callback."
  );
  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function test_title_on_initial_visit() {
  let place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_visit_title"),
    title: "My title",
    visits: [new VisitInfo()],
    guid: "mnopqrstuvwx",
  };
  let visitPromise = new Promise(resolve => {
    new VisitObserver(place.uri, place.guid, function (
      aVisitDate,
      aTransitionType,
      aLastKnownTitle
    ) {
      Assert.equal(place.title, aLastKnownTitle);

      resolve();
    });
  });
  await promiseUpdatePlaces(place);
  await visitPromise;

  // Now check an empty title doesn't get reported as null
  place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_visit_title"),
    title: "",
    visits: [new VisitInfo()],
    guid: "fghijklmnopq",
  };
  visitPromise = new Promise(resolve => {
    new VisitObserver(place.uri, place.guid, function (
      aVisitDate,
      aTransitionType,
      aLastKnownTitle
    ) {
      Assert.equal(place.title, aLastKnownTitle);

      resolve();
    });
  });
  await promiseUpdatePlaces(place);
  await visitPromise;

  // and that a missing title correctly gets reported as null.
  place = {
    uri: NetUtil.newURI(TEST_DOMAIN + "test_visit_title"),
    visits: [new VisitInfo()],
    guid: "fghijklmnopq",
  };
  visitPromise = new Promise(resolve => {
    new VisitObserver(place.uri, place.guid, function (
      aVisitDate,
      aTransitionType,
      aLastKnownTitle
    ) {
      Assert.equal(null, aLastKnownTitle);

      resolve();
    });
  });
  await promiseUpdatePlaces(place);
  await visitPromise;
});

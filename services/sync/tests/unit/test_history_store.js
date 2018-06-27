/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://services-common/utils.js");
ChromeUtils.import("resource://services-sync/engines/history.js");
ChromeUtils.import("resource://services-sync/service.js");
ChromeUtils.import("resource://services-sync/util.js");

const TIMESTAMP1 = (Date.now() - 103406528) * 1000;
const TIMESTAMP2 = (Date.now() - 6592903) * 1000;
const TIMESTAMP3 = (Date.now() - 123894) * 1000;

function promiseOnVisitObserved() {
  return new Promise(res => {
    PlacesUtils.history.addObserver({
      onBeginUpdateBatch: function onBeginUpdateBatch() {},
      onEndUpdateBatch: function onEndUpdateBatch() {},
      onPageChanged: function onPageChanged() {},
      onTitleChanged: function onTitleChanged() {
      },
      onVisits: function onVisits() {
        PlacesUtils.history.removeObserver(this);
        res();
      },
      onDeleteVisits: function onDeleteVisits() {},
      onPageExpired: function onPageExpired() {},
      onDeleteURI: function onDeleteURI() {},
      onClearHistory: function onClearHistory() {},
      QueryInterface: ChromeUtils.generateQI([
        Ci.nsINavHistoryObserver,
        Ci.nsINavHistoryObserver_MOZILLA_1_9_1_ADDITIONS,
        Ci.nsISupportsWeakReference
      ])
    }, true);
  });
}

function isDateApproximately(actual, expected, skewMillis = 1000) {
  let lowerBound = expected - skewMillis;
  let upperBound = expected + skewMillis;
  return actual >= lowerBound && actual <= upperBound;
}

let engine, store, fxuri, fxguid, tburi, tbguid;

async function applyEnsureNoFailures(records) {
  Assert.equal((await store.applyIncomingBatch(records)).length, 0);
}

add_task(async function setup() {
  engine = new HistoryEngine(Service);
  await engine.initialize();
  store = engine._store;
});

add_task(async function test_store() {
  _("Verify that we've got an empty store to work with.");
  do_check_empty((await store.getAllIDs()));

  _("Let's create an entry in the database.");
  fxuri = CommonUtils.makeURI("http://getfirefox.com/");

  await PlacesTestUtils.addVisits({ uri: fxuri, title: "Get Firefox!",
                                  visitDate: TIMESTAMP1 });
  _("Verify that the entry exists.");
  let ids = Object.keys((await store.getAllIDs()));
  Assert.equal(ids.length, 1);
  fxguid = ids[0];
  Assert.ok((await store.itemExists(fxguid)));

  _("If we query a non-existent record, it's marked as deleted.");
  let record = await store.createRecord("non-existent");
  Assert.ok(record.deleted);

  _("Verify createRecord() returns a complete record.");
  record = await store.createRecord(fxguid);
  Assert.equal(record.histUri, fxuri.spec);
  Assert.equal(record.title, "Get Firefox!");
  Assert.equal(record.visits.length, 1);
  Assert.equal(record.visits[0].date, TIMESTAMP1);
  Assert.equal(record.visits[0].type, Ci.nsINavHistoryService.TRANSITION_LINK);

  _("Let's modify the record and have the store update the database.");
  let secondvisit = {date: TIMESTAMP2,
                     type: Ci.nsINavHistoryService.TRANSITION_TYPED};
  let onVisitObserved = promiseOnVisitObserved();
  await applyEnsureNoFailures([
    {id: fxguid,
     histUri: record.histUri,
     title: "Hol Dir Firefox!",
     visits: [record.visits[0], secondvisit]}
  ]);
  await onVisitObserved;
  let queryres = await PlacesUtils.history.fetch(fxuri.spec, {
    includeVisits: true,
  });
  Assert.equal(queryres.title, "Hol Dir Firefox!");
  Assert.deepEqual(queryres.visits, [{
    date: new Date(TIMESTAMP2 / 1000),
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  }, {
    date: new Date(TIMESTAMP1 / 1000),
    transition: Ci.nsINavHistoryService.TRANSITION_LINK,
  }]);
  await PlacesUtils.history.clear();
});

add_task(async function test_store_create() {
  _("Create a brand new record through the store.");
  tbguid = Utils.makeGUID();
  tburi = CommonUtils.makeURI("http://getthunderbird.com");
  let onVisitObserved = promiseOnVisitObserved();
  await applyEnsureNoFailures([
    {id: tbguid,
     histUri: tburi.spec,
     title: "The bird is the word!",
     visits: [{date: TIMESTAMP3,
               type: Ci.nsINavHistoryService.TRANSITION_TYPED}]}
  ]);
  await onVisitObserved;
  Assert.ok((await store.itemExists(tbguid)));
  do_check_attribute_count(await store.getAllIDs(), 1);
  let queryres = await PlacesUtils.history.fetch(tburi.spec, {
    includeVisits: true,
  });
  Assert.equal(queryres.title, "The bird is the word!");
  Assert.deepEqual(queryres.visits, [{
    date: new Date(TIMESTAMP3 / 1000),
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  }]);
  await PlacesUtils.history.clear();
});

add_task(async function test_null_title() {
  _("Make sure we handle a null title gracefully (it can happen in some cases, e.g. for resource:// URLs)");
  let resguid = Utils.makeGUID();
  let resuri = CommonUtils.makeURI("unknown://title");
  await applyEnsureNoFailures([
    {id: resguid,
     histUri: resuri.spec,
     title: null,
     visits: [{date: TIMESTAMP3,
               type: Ci.nsINavHistoryService.TRANSITION_TYPED}]}
  ]);
  do_check_attribute_count((await store.getAllIDs()), 1);

  let queryres = await PlacesUtils.history.fetch(resuri.spec, {
    includeVisits: true,
  });
  Assert.equal(queryres.title, "");
  Assert.deepEqual(queryres.visits, [{
    date: new Date(TIMESTAMP3 / 1000),
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  }]);
  await PlacesUtils.history.clear();
});

add_task(async function test_invalid_records() {
  _("Make sure we handle invalid URLs in places databases gracefully.");
  await PlacesUtils.withConnectionWrapper("test_invalid_record", async function(db) {
    await db.execute(
      "INSERT INTO moz_places "
      + "(url, url_hash, title, rev_host, visit_count, last_visit_date) "
      + "VALUES ('invalid-uri', hash('invalid-uri'), 'Invalid URI', '.', 1, " + TIMESTAMP3 + ")"
    );
    // Trigger the update to the moz_origin tables by deleting the added rows
    // from moz_updateoriginsinsert_temp
    await db.executeCached("DELETE FROM moz_updateoriginsinsert_temp");
    // Add the corresponding visit to retain database coherence.
    await db.execute(
      "INSERT INTO moz_historyvisits "
      + "(place_id, visit_date, visit_type, session) "
      + "VALUES ((SELECT id FROM moz_places WHERE url_hash = hash('invalid-uri') AND url = 'invalid-uri'), "
      + TIMESTAMP3 + ", " + Ci.nsINavHistoryService.TRANSITION_TYPED + ", 1)"
    );
  });
  do_check_attribute_count((await store.getAllIDs()), 1);

  _("Make sure we report records with invalid URIs.");
  let invalid_uri_guid = Utils.makeGUID();
  let failed = await store.applyIncomingBatch([{
    id: invalid_uri_guid,
    histUri: ":::::::::::::::",
    title: "Doesn't have a valid URI",
    visits: [{date: TIMESTAMP3,
              type: Ci.nsINavHistoryService.TRANSITION_EMBED}]}
  ]);
  Assert.equal(failed.length, 1);
  Assert.equal(failed[0], invalid_uri_guid);

  _("Make sure we handle records with invalid GUIDs gracefully (ignore).");
  await applyEnsureNoFailures([
    {id: "invalid",
     histUri: "http://invalid.guid/",
     title: "Doesn't have a valid GUID",
     visits: [{date: TIMESTAMP3,
               type: Ci.nsINavHistoryService.TRANSITION_EMBED}]}
  ]);

  _("Make sure we handle records with invalid visit codes or visit dates, gracefully ignoring those visits.");
  let no_date_visit_guid = Utils.makeGUID();
  let no_type_visit_guid = Utils.makeGUID();
  let invalid_type_visit_guid = Utils.makeGUID();
  let non_integer_visit_guid = Utils.makeGUID();
  failed = await store.applyIncomingBatch([
    {id: no_date_visit_guid,
     histUri: "http://no.date.visit/",
     title: "Visit has no date",
     visits: [{type: Ci.nsINavHistoryService.TRANSITION_EMBED}]},
    {id: no_type_visit_guid,
     histUri: "http://no.type.visit/",
     title: "Visit has no type",
     visits: [{date: TIMESTAMP3}]},
    {id: invalid_type_visit_guid,
     histUri: "http://invalid.type.visit/",
     title: "Visit has invalid type",
     visits: [{date: TIMESTAMP3,
               type: Ci.nsINavHistoryService.TRANSITION_LINK - 1}]},
    {id: non_integer_visit_guid,
     histUri: "http://non.integer.visit/",
     title: "Visit has non-integer date",
     visits: [{date: 1234.567,
               type: Ci.nsINavHistoryService.TRANSITION_EMBED}]}
  ]);
  Assert.equal(failed.length, 0);

  // Make sure we can apply tombstones (both valid and invalid)
  failed = await store.applyIncomingBatch([
    {id: no_date_visit_guid,
     deleted: true},
    {id: "not-a-valid-guid",
     deleted: true},
  ]);
  Assert.deepEqual(failed, ["not-a-valid-guid"]);

  _("Make sure we handle records with javascript: URLs gracefully.");
  await applyEnsureNoFailures([
    {id: Utils.makeGUID(),
     histUri: "javascript:''",
     title: "javascript:''",
     visits: [{date: TIMESTAMP3,
               type: Ci.nsINavHistoryService.TRANSITION_EMBED}]}
  ]);

  _("Make sure we handle records without any visits gracefully.");
  await applyEnsureNoFailures([
    {id: Utils.makeGUID(),
     histUri: "http://getfirebug.com",
     title: "Get Firebug!",
     visits: []}
  ]);
});

add_task(async function test_unknowingly_invalid_records() {
  _("Make sure we handle rejection of records by places gracefully.");
  let oldCAU = store._canAddURI;
  store._canAddURI = () => true;
  try {
    _("Make sure that when places rejects this record we record it as failed");
    let guid = Utils.makeGUID();
    let result = await store.applyIncomingBatch([
      {id: guid,
       histUri: "javascript:''",
       title: "javascript:''",
       visits: [{date: TIMESTAMP3,
                 type: Ci.nsINavHistoryService.TRANSITION_EMBED}]}
    ]);
    deepEqual(result, [guid]);
  } finally {
    store._canAddURI = oldCAU;
  }
});

add_task(async function test_clamp_visit_dates() {
  let futureVisitTime = Date.now() + 5 * 60 * 1000;
  let recentVisitTime = Date.now() - 5 * 60 * 1000;

  await applyEnsureNoFailures([{
    id: "visitAAAAAAA",
    histUri: "http://example.com/a",
    title: "A",
    visits: [{
      date: "invalidDate",
      type: Ci.nsINavHistoryService.TRANSITION_LINK,
    }],
  }, {
    id: "visitBBBBBBB",
    histUri: "http://example.com/b",
    title: "B",
    visits: [{
      date: 100,
      type: Ci.nsINavHistoryService.TRANSITION_TYPED,
    }, {
      date: 250,
      type: Ci.nsINavHistoryService.TRANSITION_TYPED,
    }, {
      date: recentVisitTime * 1000,
      type: Ci.nsINavHistoryService.TRANSITION_TYPED,
    }],
  }, {
    id: "visitCCCCCCC",
    histUri: "http://example.com/c",
    title: "D",
    visits: [{
      date: futureVisitTime * 1000,
      type: Ci.nsINavHistoryService.TRANSITION_BOOKMARK,
    }],
  }, {
    id: "visitDDDDDDD",
    histUri: "http://example.com/d",
    title: "D",
    visits: [{
      date: recentVisitTime * 1000,
      type: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
    }],
  }]);

  let visitsForA = await PlacesSyncUtils.history.fetchVisitsForURL(
    "http://example.com/a");
  deepEqual(visitsForA, [], "Should ignore visits with invalid dates");

  let visitsForB = await PlacesSyncUtils.history.fetchVisitsForURL(
    "http://example.com/b");
  deepEqual(visitsForB, [{
    date: recentVisitTime * 1000,
    type: Ci.nsINavHistoryService.TRANSITION_TYPED,
  }, {
    // We should clamp visit dates older than original Mosaic release.
    date: PlacesSyncUtils.bookmarks.EARLIEST_BOOKMARK_TIMESTAMP * 1000,
    type: Ci.nsINavHistoryService.TRANSITION_TYPED,
  }], "Should record clamped visit and valid visit for B");

  let visitsForC = await PlacesSyncUtils.history.fetchVisitsForURL(
    "http://example.com/c");
  equal(visitsForC.length, 1, "Should record clamped future visit for C");
  let visitDateForC = PlacesUtils.toDate(visitsForC[0].date);
  ok(isDateApproximately(visitDateForC, Date.now()),
    "Should clamp future visit date for C to now");

  let visitsForD = await PlacesSyncUtils.history.fetchVisitsForURL(
    "http://example.com/d");
  deepEqual(visitsForD, [{
    date: recentVisitTime * 1000,
    type: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
  }], "Should not clamp valid visit dates");
});

add_task(async function test_remove() {
  _("Remove an existent record and a non-existent from the store.");
  await applyEnsureNoFailures([{id: fxguid, deleted: true},
                         {id: Utils.makeGUID(), deleted: true}]);
  Assert.equal(false, (await store.itemExists(fxguid)));
  let queryres = await PlacesUtils.history.fetch(fxuri.spec, {
    includeVisits: true,
  });
  Assert.equal(null, queryres);

  _("Make sure wipe works.");
  await store.wipe();
  do_check_empty((await store.getAllIDs()));
  queryres = await PlacesUtils.history.fetch(fxuri.spec, {
    includeVisits: true,
  });
  Assert.equal(null, queryres);
  queryres = await PlacesUtils.history.fetch(tburi.spec, {
    includeVisits: true,
  });
  Assert.equal(null, queryres);
});

add_task(async function test_chunking() {
  let mvpi = store.MAX_VISITS_PER_INSERT;
  store.MAX_VISITS_PER_INSERT = 3;
  let checkChunks = function(input, expected) {
    let chunks = Array.from(store._generateChunks(input));
    deepEqual(chunks, expected);
  };
  try {
    checkChunks([{visits: ["x"]}],
                [[{visits: ["x"]}]]);

    // 3 should still be one chunk.
    checkChunks([{visits: ["x", "x", "x"]}],
                [[{visits: ["x", "x", "x"]}]]);

    // 4 should still be one chunk as we don't split individual records.
    checkChunks([{visits: ["x", "x", "x", "x"]}],
                [[{visits: ["x", "x", "x", "x"]}]]
               );

    // 4 in the first and 1 in the second should be 2 chunks.
    checkChunks([
                  {visits: ["x", "x", "x", "x"]},
                  {visits: ["x"]}
                ],
                // expected
                [
                  [
                    {visits: ["x", "x", "x", "x"]}
                  ],
                  [
                    {visits: ["x"]}
                  ],
                ]
               );

    // we put multiple records into chunks
    checkChunks([
                  {visits: ["x", "x"]},
                  {visits: ["x"]},
                  {visits: ["x"]},
                  {visits: ["x", "x"]},
                  {visits: ["x", "x", "x", "x"]},
                ],
                // expected
                [
                  [
                    {visits: ["x", "x"]},
                    {visits: ["x"]},
                  ],
                  [
                    {visits: ["x"]},
                    {visits: ["x", "x"]},
                  ],
                  [
                    {visits: ["x", "x", "x", "x"]},
                  ],
                ]
               );
  } finally {
    store.MAX_VISITS_PER_INSERT = mvpi;
  }
});

add_task(async function test_getAllIDs_filters_file_uris() {
  let uri = CommonUtils.makeURI("file:///Users/eoger/tps/config.json");
  let visitAddedPromise = promiseVisit("added", uri);
  await PlacesTestUtils.addVisits({
    uri,
    visitDate: Date.now() * 1000,
    transition: PlacesUtils.history.TRANSITION_LINK
  });
  await visitAddedPromise;

  do_check_attribute_count(await store.getAllIDs(), 0);

  await PlacesUtils.history.clear();
});

add_task(async function test_applyIncomingBatch_filters_file_uris() {
  const guid = Utils.makeGUID();
  let uri = CommonUtils.makeURI("file:///Users/eoger/tps/config.json");
  await applyEnsureNoFailures([
    {id: guid,
     histUri: uri.spec,
     title: "TPS CONFIG",
     visits: [{date: TIMESTAMP3,
               type: Ci.nsINavHistoryService.TRANSITION_TYPED}]}
  ]);
  Assert.equal(false, (await store.itemExists(guid)));
  let queryres = await PlacesUtils.history.fetch(uri.spec, {
    includeVisits: true,
  });
  Assert.equal(null, queryres);
});

add_task(async function cleanup() {
  _("Clean up.");
  await PlacesUtils.history.clear();
});

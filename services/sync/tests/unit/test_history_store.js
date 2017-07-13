/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://testing-common/PlacesTestUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/engines/history.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

const TIMESTAMP1 = (Date.now() - 103406528) * 1000;
const TIMESTAMP2 = (Date.now() - 6592903) * 1000;
const TIMESTAMP3 = (Date.now() - 123894) * 1000;

function queryPlaces(uri, options) {
  let query = PlacesUtils.history.getNewQuery();
  query.uri = uri;
  let res = PlacesUtils.history.executeQuery(query, options);
  res.root.containerOpen = true;

  let results = [];
  for (let i = 0; i < res.root.childCount; i++)
    results.push(res.root.getChild(i));
  res.root.containerOpen = false;
  return results;
}

function queryHistoryVisits(uri) {
  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY;
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  options.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_ASCENDING;
  return queryPlaces(uri, options);
}

function promiseOnVisitObserved() {
  return new Promise(res => {
    PlacesUtils.history.addObserver({
      onBeginUpdateBatch: function onBeginUpdateBatch() {},
      onEndUpdateBatch: function onEndUpdateBatch() {},
      onPageChanged: function onPageChanged() {},
      onTitleChanged: function onTitleChanged() {
      },
      onVisit: function onVisit() {
        PlacesUtils.history.removeObserver(this);
        res();
      },
      onDeleteVisits: function onDeleteVisits() {},
      onPageExpired: function onPageExpired() {},
      onDeleteURI: function onDeleteURI() {},
      onClearHistory: function onClearHistory() {},
      QueryInterface: XPCOMUtils.generateQI([
        Ci.nsINavHistoryObserver,
        Ci.nsINavHistoryObserver_MOZILLA_1_9_1_ADDITIONS,
        Ci.nsISupportsWeakReference
      ])
    }, true);
  });
}

var engine = new HistoryEngine(Service);
Async.promiseSpinningly(engine.initialize());
var store = engine._store;
async function applyEnsureNoFailures(records) {
  do_check_eq((await store.applyIncomingBatch(records)).length, 0);
}

var fxuri, fxguid, tburi, tbguid;

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

add_task(async function test_store() {
  _("Verify that we've got an empty store to work with.");
  do_check_empty((await store.getAllIDs()));

  _("Let's create an entry in the database.");
  fxuri = Utils.makeURI("http://getfirefox.com/");

  await PlacesTestUtils.addVisits({ uri: fxuri, title: "Get Firefox!",
                                  visitDate: TIMESTAMP1 });
  _("Verify that the entry exists.");
  let ids = Object.keys((await store.getAllIDs()));
  do_check_eq(ids.length, 1);
  fxguid = ids[0];
  do_check_true((await store.itemExists(fxguid)));

  _("If we query a non-existent record, it's marked as deleted.");
  let record = await store.createRecord("non-existent");
  do_check_true(record.deleted);

  _("Verify createRecord() returns a complete record.");
  record = await store.createRecord(fxguid);
  do_check_eq(record.histUri, fxuri.spec);
  do_check_eq(record.title, "Get Firefox!");
  do_check_eq(record.visits.length, 1);
  do_check_eq(record.visits[0].date, TIMESTAMP1);
  do_check_eq(record.visits[0].type, Ci.nsINavHistoryService.TRANSITION_LINK);

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
  try {
    let queryres = queryHistoryVisits(fxuri);
    do_check_eq(queryres.length, 2);
    do_check_eq(queryres[0].time, TIMESTAMP1);
    do_check_eq(queryres[0].title, "Hol Dir Firefox!");
    do_check_eq(queryres[1].time, TIMESTAMP2);
    do_check_eq(queryres[1].title, "Hol Dir Firefox!");
  } catch (ex) {
    PlacesTestUtils.clearHistory();
    do_throw(ex);
  }
});

add_task(async function test_store_create() {
  _("Create a brand new record through the store.");
  tbguid = Utils.makeGUID();
  tburi = Utils.makeURI("http://getthunderbird.com");
  let onVisitObserved = promiseOnVisitObserved();
  await applyEnsureNoFailures([
    {id: tbguid,
     histUri: tburi.spec,
     title: "The bird is the word!",
     visits: [{date: TIMESTAMP3,
               type: Ci.nsINavHistoryService.TRANSITION_TYPED}]}
  ]);
  await onVisitObserved;
  try {
    do_check_attribute_count(Async.promiseSpinningly(store.getAllIDs()), 2);
    let queryres = queryHistoryVisits(tburi);
    do_check_eq(queryres.length, 1);
    do_check_eq(queryres[0].time, TIMESTAMP3);
    do_check_eq(queryres[0].title, "The bird is the word!");
  } catch (ex) {
    PlacesTestUtils.clearHistory();
    do_throw(ex);
  }
});

add_task(async function test_null_title() {
  _("Make sure we handle a null title gracefully (it can happen in some cases, e.g. for resource:// URLs)");
  let resguid = Utils.makeGUID();
  let resuri = Utils.makeURI("unknown://title");
  await applyEnsureNoFailures([
    {id: resguid,
     histUri: resuri.spec,
     title: null,
     visits: [{date: TIMESTAMP3,
               type: Ci.nsINavHistoryService.TRANSITION_TYPED}]}
  ]);
  do_check_attribute_count((await store.getAllIDs()), 3);
  let queryres = queryHistoryVisits(resuri);
  do_check_eq(queryres.length, 1);
  do_check_eq(queryres[0].time, TIMESTAMP3);
});

add_task(async function test_invalid_records() {
  _("Make sure we handle invalid URLs in places databases gracefully.");
  let connection = PlacesUtils.history
                              .QueryInterface(Ci.nsPIPlacesDatabase)
                              .DBConnection;
  let stmt = connection.createAsyncStatement(
    "INSERT INTO moz_places "
  + "(url, url_hash, title, rev_host, visit_count, last_visit_date) "
  + "VALUES ('invalid-uri', hash('invalid-uri'), 'Invalid URI', '.', 1, " + TIMESTAMP3 + ")"
  );
  Async.querySpinningly(stmt);
  stmt.finalize();
  // Add the corresponding visit to retain database coherence.
  stmt = connection.createAsyncStatement(
    "INSERT INTO moz_historyvisits "
  + "(place_id, visit_date, visit_type, session) "
  + "VALUES ((SELECT id FROM moz_places WHERE url_hash = hash('invalid-uri') AND url = 'invalid-uri'), "
  + TIMESTAMP3 + ", " + Ci.nsINavHistoryService.TRANSITION_TYPED + ", 1)"
  );
  Async.querySpinningly(stmt);
  stmt.finalize();
  do_check_attribute_count((await store.getAllIDs()), 4);

  _("Make sure we report records with invalid URIs.");
  let invalid_uri_guid = Utils.makeGUID();
  let failed = await store.applyIncomingBatch([{
    id: invalid_uri_guid,
    histUri: ":::::::::::::::",
    title: "Doesn't have a valid URI",
    visits: [{date: TIMESTAMP3,
              type: Ci.nsINavHistoryService.TRANSITION_EMBED}]}
  ]);
  do_check_eq(failed.length, 1);
  do_check_eq(failed[0], invalid_uri_guid);

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
  do_check_eq(failed.length, 0);

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

add_task(async function test_remove() {
  _("Remove an existent record and a non-existent from the store.");
  await applyEnsureNoFailures([{id: fxguid, deleted: true},
                         {id: Utils.makeGUID(), deleted: true}]);
  do_check_false((await store.itemExists(fxguid)));
  let queryres = queryHistoryVisits(fxuri);
  do_check_eq(queryres.length, 0);

  _("Make sure wipe works.");
  await store.wipe();
  do_check_empty((await store.getAllIDs()));
  queryres = queryHistoryVisits(fxuri);
  do_check_eq(queryres.length, 0);
  queryres = queryHistoryVisits(tburi);
  do_check_eq(queryres.length, 0);
});

add_test(function cleanup() {
  _("Clean up.");
  PlacesTestUtils.clearHistory().then(run_next_test);
});

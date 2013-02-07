/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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

function onNextTitleChanged(callback) {
  PlacesUtils.history.addObserver({
    onBeginUpdateBatch: function onBeginUpdateBatch() {},
    onEndUpdateBatch: function onEndUpdateBatch() {},
    onPageChanged: function onPageChanged() {},
    onTitleChanged: function onTitleChanged() {
      PlacesUtils.history.removeObserver(this);
      Utils.nextTick(callback);
    },
    onVisit: function onVisit() {},
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
}

// Ensure exceptions from inside callbacks leads to test failures while
// we still clean up properly.
function ensureThrows(func) {
  return function() {
    try {
      func.apply(this, arguments);
    } catch (ex) {
      PlacesUtils.history.removeAllPages();
      do_throw(ex);
    }
  };
}

let store = new HistoryEngine(Service)._store;
function applyEnsureNoFailures(records) {
  do_check_eq(store.applyIncomingBatch(records).length, 0);
}

let fxuri, fxguid, tburi, tbguid;

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

add_test(function test_store() {
  _("Verify that we've got an empty store to work with.");
  do_check_empty(store.getAllIDs());

  _("Let's create an entry in the database.");
  fxuri = Utils.makeURI("http://getfirefox.com/");

  let place = {
    uri: fxuri,
    title: "Get Firefox!",
    visits: [{
      visitDate: TIMESTAMP1,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK
    }]
  };
  PlacesUtils.asyncHistory.updatePlaces(place, {
    handleError: function handleError() {
      do_throw("Unexpected error in adding visit.");
    },
    handleResult: function handleResult() {},
    handleCompletion: onVisitAdded
  });

  function onVisitAdded() {
    _("Verify that the entry exists.");
    let ids = Object.keys(store.getAllIDs());
    do_check_eq(ids.length, 1);
    fxguid = ids[0];
    do_check_true(store.itemExists(fxguid));

    _("If we query a non-existent record, it's marked as deleted.");
    let record = store.createRecord("non-existent");
    do_check_true(record.deleted);

    _("Verify createRecord() returns a complete record.");
    record = store.createRecord(fxguid);
    do_check_eq(record.histUri, fxuri.spec);
    do_check_eq(record.title, "Get Firefox!");
    do_check_eq(record.visits.length, 1);
    do_check_eq(record.visits[0].date, TIMESTAMP1);
    do_check_eq(record.visits[0].type, Ci.nsINavHistoryService.TRANSITION_LINK);

    _("Let's modify the record and have the store update the database.");
    let secondvisit = {date: TIMESTAMP2,
                       type: Ci.nsINavHistoryService.TRANSITION_TYPED};
    onNextTitleChanged(ensureThrows(function() {
      let queryres = queryHistoryVisits(fxuri);
      do_check_eq(queryres.length, 2);
      do_check_eq(queryres[0].time, TIMESTAMP1);
      do_check_eq(queryres[0].title, "Hol Dir Firefox!");
      do_check_eq(queryres[1].time, TIMESTAMP2);
      do_check_eq(queryres[1].title, "Hol Dir Firefox!");
      run_next_test();
    }));
    applyEnsureNoFailures([
      {id: fxguid,
       histUri: record.histUri,
       title: "Hol Dir Firefox!",
       visits: [record.visits[0], secondvisit]}
    ]);
  }
});

add_test(function test_store_create() {
  _("Create a brand new record through the store.");
  tbguid = Utils.makeGUID();
  tburi = Utils.makeURI("http://getthunderbird.com");
  onNextTitleChanged(ensureThrows(function() {
    do_check_attribute_count(store.getAllIDs(), 2);
    let queryres = queryHistoryVisits(tburi);
    do_check_eq(queryres.length, 1);
    do_check_eq(queryres[0].time, TIMESTAMP3);
    do_check_eq(queryres[0].title, "The bird is the word!");
    run_next_test();
  }));
  applyEnsureNoFailures([
    {id: tbguid,
     histUri: tburi.spec,
     title: "The bird is the word!",
     visits: [{date: TIMESTAMP3,
               type: Ci.nsINavHistoryService.TRANSITION_TYPED}]}
  ]);
});

add_test(function test_null_title() {
  _("Make sure we handle a null title gracefully (it can happen in some cases, e.g. for resource:// URLs)");
  let resguid = Utils.makeGUID();
  let resuri = Utils.makeURI("unknown://title");
  applyEnsureNoFailures([
    {id: resguid,
     histUri: resuri.spec,
     title: null,
     visits: [{date: TIMESTAMP3,
               type: Ci.nsINavHistoryService.TRANSITION_TYPED}]}
  ]);
  do_check_attribute_count(store.getAllIDs(), 3);
  let queryres = queryHistoryVisits(resuri);
  do_check_eq(queryres.length, 1);
  do_check_eq(queryres[0].time, TIMESTAMP3);
  run_next_test();
});

add_test(function test_invalid_records() {
  _("Make sure we handle invalid URLs in places databases gracefully.");
  let connection = PlacesUtils.history
                              .QueryInterface(Ci.nsPIPlacesDatabase)
                              .DBConnection;
  let stmt = connection.createAsyncStatement(
    "INSERT INTO moz_places "
  + "(url, title, rev_host, visit_count, last_visit_date) "
  + "VALUES ('invalid-uri', 'Invalid URI', '.', 1, " + TIMESTAMP3 + ")"
  );
  Async.querySpinningly(stmt);
  stmt.finalize();
  // Add the corresponding visit to retain database coherence.
  stmt = connection.createAsyncStatement(
    "INSERT INTO moz_historyvisits "
  + "(place_id, visit_date, visit_type, session) "
  + "VALUES ((SELECT id FROM moz_places WHERE url = 'invalid-uri'), "
  + TIMESTAMP3 + ", " + Ci.nsINavHistoryService.TRANSITION_TYPED + ", 1)"
  );
  Async.querySpinningly(stmt);
  stmt.finalize();
  do_check_attribute_count(store.getAllIDs(), 4);

  _("Make sure we report records with invalid URIs.");
  let invalid_uri_guid = Utils.makeGUID();
  let failed = store.applyIncomingBatch([{
    id: invalid_uri_guid,
    histUri: ":::::::::::::::",
    title: "Doesn't have a valid URI",
    visits: [{date: TIMESTAMP3,
              type: Ci.nsINavHistoryService.TRANSITION_EMBED}]}
  ]);
  do_check_eq(failed.length, 1);
  do_check_eq(failed[0], invalid_uri_guid);

  _("Make sure we handle records with invalid GUIDs gracefully (ignore).");
  applyEnsureNoFailures([
    {id: "invalid",
     histUri: "http://invalid.guid/",
     title: "Doesn't have a valid GUID",
     visits: [{date: TIMESTAMP3,
               type: Ci.nsINavHistoryService.TRANSITION_EMBED}]}
  ]);

  _("Make sure we report records with invalid visits, gracefully handle non-integer dates.");
  let no_date_visit_guid = Utils.makeGUID();
  let no_type_visit_guid = Utils.makeGUID();
  let invalid_type_visit_guid = Utils.makeGUID();
  let non_integer_visit_guid = Utils.makeGUID();
  failed = store.applyIncomingBatch([
    {id: no_date_visit_guid,
     histUri: "http://no.date.visit/",
     title: "Visit has no date",
     visits: [{date: TIMESTAMP3}]},
    {id: no_type_visit_guid,
     histUri: "http://no.type.visit/",
     title: "Visit has no type",
     visits: [{type: Ci.nsINavHistoryService.TRANSITION_EMBED}]},
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
  do_check_eq(failed.length, 3);
  failed.sort();
  let expected = [no_date_visit_guid,
                  no_type_visit_guid,
                  invalid_type_visit_guid].sort();
  for (let i = 0; i < expected.length; i++) {
    do_check_eq(failed[i], expected[i]);
  }

  _("Make sure we handle records with javascript: URLs gracefully.");
  applyEnsureNoFailures([
    {id: Utils.makeGUID(),
     histUri: "javascript:''",
     title: "javascript:''",
     visits: [{date: TIMESTAMP3,
               type: Ci.nsINavHistoryService.TRANSITION_EMBED}]}
  ]);

  _("Make sure we handle records without any visits gracefully.");
  applyEnsureNoFailures([
    {id: Utils.makeGUID(),
     histUri: "http://getfirebug.com",
     title: "Get Firebug!",
     visits: []}
  ]);

  run_next_test();
});

add_test(function test_remove() {
  _("Remove an existent record and a non-existent from the store.");
  applyEnsureNoFailures([{id: fxguid, deleted: true},
                         {id: Utils.makeGUID(), deleted: true}]);
  do_check_false(store.itemExists(fxguid));
  let queryres = queryHistoryVisits(fxuri);
  do_check_eq(queryres.length, 0);

  _("Make sure wipe works.");
  store.wipe();
  do_check_empty(store.getAllIDs());
  queryres = queryHistoryVisits(fxuri);
  do_check_eq(queryres.length, 0);
  queryres = queryHistoryVisits(tburi);
  do_check_eq(queryres.length, 0);
  run_next_test();
});

add_test(function cleanup() {
  _("Clean up.");
  PlacesUtils.history.removeAllPages();
  run_next_test();
});

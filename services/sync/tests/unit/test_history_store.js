Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines/history.js");
Cu.import("resource://services-sync/type_records/history.js");
Cu.import("resource://services-sync/util.js");

const TIMESTAMP1 = (Date.now() - 103406528) * 1000;
const TIMESTAMP2 = (Date.now() - 6592903) * 1000;
const TIMESTAMP3 = (Date.now() - 123894) * 1000;

function queryPlaces(uri, options) {
  let query = Svc.History.getNewQuery();
  query.uri = uri;
  let res = Svc.History.executeQuery(query, options);
  res.root.containerOpen = true;

  let results = [];
  for (let i = 0; i < res.root.childCount; i++)
    results.push(res.root.getChild(i));
  return results;
}

function queryHistoryVisits(uri) {
  let options = Svc.History.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY;
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  options.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_ASCENDING;
  return queryPlaces(uri, options);
}

function onNextTitleChanged(callback) {
  Svc.History.addObserver({
    onBeginUpdateBatch: function onBeginUpdateBatch() {},
    onEndUpdateBatch: function onEndUpdateBatch() {},
    onPageChanged: function onPageChanged() {},
    onTitleChanged: function onTitleChanged() {
      Svc.History.removeObserver(this);
      Utils.delay(callback, 0, this);
    },
    onVisit: function onVisit() {},
    onDeleteVisits: function onDeleteVisits() {},
    onPageExpired: function onPageExpired() {},
    onBeforeDeleteURI: function onBeforeDeleteURI() {},
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
      Svc.History.removeAllPages();
      do_throw(ex);
    }
  };
}

function run_test() {
  _("Verify that we've got an empty store to work with.");
  let store = new HistoryEngine()._store;
  do_check_eq([id for (id in store.getAllIDs())].length, 0);

  let fxuri, fxguid, tburi, tbguid;
  do_test_pending();
  Utils.asyncChain(function (next) {

    _("Let's create an entry in the database.");
    fxuri = Utils.makeURI("http://getfirefox.com/");
    Svc.History.addPageWithDetails(fxuri, "Get Firefox!", TIMESTAMP1);

    _("Verify that the entry exists.");
    let ids = [id for (id in store.getAllIDs())];
    do_check_eq(ids.length, 1);
    fxguid = ids[0];
    do_check_true(store.itemExists(fxguid));

    _("If we query a non-existent record, it's marked as deleted.");
    let record = store.createRecord("non-existent", "http://fake/uri");
    do_check_true(record.deleted);

    _("Verify createRecord() returns a complete record.");
    record = store.createRecord(fxguid, "http://fake/urk");
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
      next();
    }));
    store.update({histUri: record.histUri,
                  title: "Hol Dir Firefox!",
                  visits: [record.visits[0], secondvisit]});

  }, function (next) {

    _("Create a brand new record through the store.");
    tbguid = Utils.makeGUID();
    tburi = Utils.makeURI("http://getthunderbird.com");
    onNextTitleChanged(ensureThrows(function() {
      do_check_eq([id for (id in store.getAllIDs())].length, 2);
      let queryres = queryHistoryVisits(tburi);
      do_check_eq(queryres.length, 1);
      do_check_eq(queryres[0].time, TIMESTAMP3);
      do_check_eq(queryres[0].title, "The bird is the word!");
      next();
    }));
    store.create({id: tbguid,
                  histUri: tburi.spec,
                  title: "The bird is the word!",
                  visits: [{date: TIMESTAMP3,
                            type: Ci.nsINavHistoryService.TRANSITION_TYPED}]});

  }, function (next) {

    _("Make sure we handle invalid URLs in places databases gracefully.");
    let table = store._haveTempTables ? "moz_places_temp" : "moz_places";
    let query = "INSERT INTO " + table + " "
      + "(url, title, rev_host, visit_count, last_visit_date) "
      + "VALUES ('invalid-uri', 'Invalid URI', '.', 1, " + TIMESTAMP3 + ")";
    let stmt = Utils.createStatement(Svc.History.DBConnection, query);
    let result = Utils.queryAsync(stmt);    
    do_check_eq([id for (id in store.getAllIDs())].length, 3);

    _("Remove a record from the store.");
    store.remove({id: fxguid});
    do_check_false(store.itemExists(fxguid));
    let queryres = queryHistoryVisits(fxuri);
    do_check_eq(queryres.length, 0);

    _("Make sure wipe works.");
    store.wipe();
    do_check_eq([id for (id in store.getAllIDs())].length, 0);
    queryres = queryHistoryVisits(fxuri);
    do_check_eq(queryres.length, 0);
    queryres = queryHistoryVisits(tburi);
    do_check_eq(queryres.length, 0);

    _("Clean up.");
    Svc.History.removeAllPages();
    do_test_finished();

  })();

}

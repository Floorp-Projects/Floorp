/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Generic nsINavHistoryObserver that doesn't implement anything, but provides
 * dummy methods to prevent errors about an object not having a certain method.
 */
function NavHistoryObserver() {
}
NavHistoryObserver.prototype = {
  onBeginUpdateBatch: function() { },
  onEndUpdateBatch: function() { },
  onVisit: function() { },
  onTitleChanged: function() { },
  onBeforeDeleteURI: function() { },
  onDeleteURI: function() { },
  onClearHistory: function() { },
  onPageChanged: function() { },
  onDeleteVisits: function() { },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
};

/**
 * Registers a one-time history observer for and calls the callback
 * when the specified nsINavHistoryObserver method is called.
 */
function onNotify(callback) {
  let obs = new NavHistoryObserver();
  obs[callback.name] = function () {
    PlacesUtils.history.removeObserver(this);
    callback.apply(this, arguments);
  };
  PlacesUtils.history.addObserver(obs, false);
}

/**
 * Adds a TRANSITION_TYPED visit to the history database.
 */
function add_visit(uri, timestamp) {
  uri = uri || NetUtil.newURI("http://firefox.com/");
  timestamp = timestamp || Date.now() * 1000;
  PlacesUtils.history.addVisit(
    uri, timestamp, null, Ci.nsINavHistoryService.TRANSITION_TYPED, false, 0);
  return [uri, timestamp];
}

function run_test() {
  run_next_test();
}

add_test(function test_onVisit() {
  onNotify(function onVisit(aURI, aVisitID, aTime, aSessionID, aReferringID,
                            aTransitionType, aGUID) {
    do_check_true(aURI.equals(testuri));
    do_check_true(aVisitID > 0);
    do_check_eq(aTime, testtime);
    do_check_eq(aSessionID, 0);
    do_check_eq(aReferringID, 0);
    do_check_eq(aTransitionType, Ci.nsINavHistoryService.TRANSITION_TYPED);
    do_check_guid_for_uri(aURI, aGUID);

    run_next_test();
  });
  let testuri = NetUtil.newURI("http://firefox.com/");
  let testtime = Date.now() * 1000;
  add_visit(testuri, testtime);
});

add_test(function test_onBeforeDeleteURI() {
  onNotify(function onBeforeDeleteURI(aURI, aGUID) {
    do_check_true(aURI.equals(testuri));
    do_check_guid_for_uri(aURI, aGUID);

    run_next_test();
  });
  let [testuri] = add_visit();
  PlacesUtils.bhistory.removePage(testuri);
});

add_test(function test_onDeleteURI() {
  onNotify(function onDeleteURI(aURI, aGUID) {
    do_check_true(aURI.equals(testuri));
    // Can't use do_check_guid_for_uri() here because the visit is already gone.
    do_check_eq(aGUID, testguid);

    run_next_test();
  });
  let [testuri] = add_visit();
  let testguid = do_get_guid_for_uri(testuri);
  PlacesUtils.bhistory.removePage(testuri);
});

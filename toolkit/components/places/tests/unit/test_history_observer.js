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
  onDeleteURI: function() { },
  onClearHistory: function() { },
  onPageChanged: function() { },
  onDeleteVisits: function() { },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
};

/**
 * Registers a one-time history observer for and calls the callback
 * when the specified nsINavHistoryObserver method is called.
 * Returns a promise that is resolved when the callback returns.
 */
function onNotify(callback) {
  let deferred = Promise.defer();
  let obs = new NavHistoryObserver();
  obs[callback.name] = function () {
    PlacesUtils.history.removeObserver(this);
    callback.apply(this, arguments);
    deferred.resolve();
  };
  PlacesUtils.history.addObserver(obs, false);
  return deferred.promise;
}

/**
 * Asynchronous task that adds a visit to the history database.
 */
function task_add_visit(uri, timestamp, transition) {
  uri = uri || NetUtil.newURI("http://firefox.com/");
  timestamp = timestamp || Date.now() * 1000;
  yield promiseAddVisits({
    uri: uri,
    transition: transition || TRANSITION_TYPED,
    visitDate: timestamp
  });
  throw new Task.Result([uri, timestamp]);
}

function run_test() {
  run_next_test();
}

add_task(function test_onVisit() {
  let promiseNotify = onNotify(function onVisit(aURI, aVisitID, aTime,
                                                aSessionID, aReferringID,
                                                aTransitionType, aGUID,
                                                aHidden) {
    do_check_true(aURI.equals(testuri));
    do_check_true(aVisitID > 0);
    do_check_eq(aTime, testtime);
    do_check_eq(aSessionID, 0);
    do_check_eq(aReferringID, 0);
    do_check_eq(aTransitionType, TRANSITION_TYPED);
    do_check_guid_for_uri(aURI, aGUID);
    do_check_false(aHidden);
  });
  let testuri = NetUtil.newURI("http://firefox.com/");
  let testtime = Date.now() * 1000;
  yield task_add_visit(testuri, testtime);
  yield promiseNotify;
});

add_task(function test_onVisit() {
  let promiseNotify = onNotify(function onVisit(aURI, aVisitID, aTime,
                                                aSessionID, aReferringID,
                                                aTransitionType, aGUID,
                                                aHidden) {
    do_check_true(aURI.equals(testuri));
    do_check_true(aVisitID > 0);
    do_check_eq(aTime, testtime);
    do_check_eq(aSessionID, 0);
    do_check_eq(aReferringID, 0);
    do_check_eq(aTransitionType, TRANSITION_FRAMED_LINK);
    do_check_guid_for_uri(aURI, aGUID);
    do_check_true(aHidden);
  });
  let testuri = NetUtil.newURI("http://hidden.firefox.com/");
  let testtime = Date.now() * 1000;
  yield task_add_visit(testuri, testtime, TRANSITION_FRAMED_LINK);
  yield promiseNotify;
});

add_task(function test_onDeleteURI() {
  let promiseNotify = onNotify(function onDeleteURI(aURI, aGUID, aReason) {
    do_check_true(aURI.equals(testuri));
    // Can't use do_check_guid_for_uri() here because the visit is already gone.
    do_check_eq(aGUID, testguid);
    do_check_eq(aReason, Ci.nsINavHistoryObserver.REASON_DELETED);
  });
  let [testuri] = yield task_add_visit();
  let testguid = do_get_guid_for_uri(testuri);
  PlacesUtils.bhistory.removePage(testuri);
  yield promiseNotify;
});

add_task(function test_onDeleteVisits() {
  let promiseNotify = onNotify(function onDeleteVisits(aURI, aVisitTime, aGUID,
                                                       aReason) {
    do_check_true(aURI.equals(testuri));
    // Can't use do_check_guid_for_uri() here because the visit is already gone.
    do_check_eq(aGUID, testguid);
    do_check_eq(aReason, Ci.nsINavHistoryObserver.REASON_DELETED);
    do_check_eq(aVisitTime, 0); // All visits have been removed.
  });
  let msecs24hrsAgo = Date.now() - (86400 * 1000);
  let [testuri] = yield task_add_visit(undefined, msecs24hrsAgo * 1000);
  // Add a bookmark so the page is not removed.
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       testuri,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "test");
  let testguid = do_get_guid_for_uri(testuri);
  PlacesUtils.bhistory.removePage(testuri);
  yield promiseNotify;
});

add_task(function test_onTitleChanged() {
  let promiseNotify = onNotify(function onTitleChanged(aURI, aTitle, aGUID) {
    do_check_true(aURI.equals(testuri));
    do_check_eq(aTitle, title);
    do_check_guid_for_uri(aURI, aGUID);
  });

  let [testuri] = yield task_add_visit();
  let title = "test-title";
  yield promiseAddVisits({
    uri: testuri,
    title: title
  });
  yield promiseNotify;
});

add_task(function test_onPageChanged() {
  let promiseNotify = onNotify(function onPageChanged(aURI, aChangedAttribute,
                                                      aNewValue, aGUID) {
    do_check_eq(aChangedAttribute, Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON);
    do_check_true(aURI.equals(testuri));
    do_check_eq(aNewValue, SMALLPNG_DATA_URI.spec);
    do_check_guid_for_uri(aURI, aGUID);
  });

  let [testuri] = yield task_add_visit();

  // The new favicon for the page must have data associated with it in order to
  // receive the onPageChanged notification.  To keep this test self-contained,
  // we use an URI representing the smallest possible PNG file.
  PlacesUtils.favicons.setAndFetchFaviconForPage(testuri, SMALLPNG_DATA_URI,
                                                 false,
                                                 PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
                                                 null);
  yield promiseNotify;
});

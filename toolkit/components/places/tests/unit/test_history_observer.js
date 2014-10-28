/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Generic nsINavHistoryObserver that doesn't implement anything, but provides
 * dummy methods to prevent errors about an object not having a certain method.
 */
function NavHistoryObserver() {
}
NavHistoryObserver.prototype = {
  onBeginUpdateBatch() { },
  onEndUpdateBatch() { },
  onVisit() { },
  onTitleChanged() { },
  onDeleteURI() { },
  onClearHistory() { },
  onPageChanged() { },
  onDeleteVisits() { },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
};

/**
 * Registers a one-time history observer for and calls the callback
 * when the specified nsINavHistoryObserver method is called.
 * Returns a promise that is resolved when the callback returns.
 */
function onNotify(callback) {
  return new Promise(resolve => {
    let obs = new NavHistoryObserver();
    obs[callback.name] = function() {
      PlacesUtils.history.removeObserver(this);
      callback.apply(this, arguments);
      resolve();
    };
    PlacesUtils.history.addObserver(obs, false);
  });
}

/**
 * Asynchronous task that adds a visit to the history database.
 */
function* task_add_visit(uri, timestamp, transition) {
  uri = uri || NetUtil.newURI("http://firefox.com/");
  timestamp = timestamp || Date.now() * 1000;
  yield PlacesTestUtils.addVisits({
    uri,
    transition: transition || TRANSITION_TYPED,
    visitDate: timestamp
  });
  return [uri, timestamp];
}

add_task(function* test_onVisit() {
  let promiseNotify = onNotify(function onVisit(aURI, aVisitID, aTime,
                                                aSessionID, aReferringID,
                                                aTransitionType, aGUID,
                                                aHidden, aVisitCount, aTyped) {
    Assert.ok(aURI.equals(testuri));
    Assert.ok(aVisitID > 0);
    Assert.equal(aTime, testtime);
    Assert.equal(aSessionID, 0);
    Assert.equal(aReferringID, 0);
    Assert.equal(aTransitionType, TRANSITION_TYPED);
    do_check_guid_for_uri(aURI, aGUID);
    Assert.ok(!aHidden);
    Assert.equal(aVisitCount, 1);
    Assert.equal(aTyped, 1);
  });
  let testuri = NetUtil.newURI("http://firefox.com/");
  let testtime = Date.now() * 1000;
  yield task_add_visit(testuri, testtime);
  yield promiseNotify;
});

add_task(function* test_onVisit() {
  let promiseNotify = onNotify(function onVisit(aURI, aVisitID, aTime,
                                                aSessionID, aReferringID,
                                                aTransitionType, aGUID,
                                                aHidden, aVisitCount, aTyped) {
    Assert.ok(aURI.equals(testuri));
    Assert.ok(aVisitID > 0);
    Assert.equal(aTime, testtime);
    Assert.equal(aSessionID, 0);
    Assert.equal(aReferringID, 0);
    Assert.equal(aTransitionType, TRANSITION_FRAMED_LINK);
    do_check_guid_for_uri(aURI, aGUID);
    Assert.ok(aHidden);
    Assert.equal(aVisitCount, 1);
    Assert.equal(aTyped, 0);
  });
  let testuri = NetUtil.newURI("http://hidden.firefox.com/");
  let testtime = Date.now() * 1000;
  yield task_add_visit(testuri, testtime, TRANSITION_FRAMED_LINK);
  yield promiseNotify;
});

add_task(function* test_multiple_onVisit() {
  let testuri = NetUtil.newURI("http://self.firefox.com/");
  let promiseNotifications = new Promise(resolve => {
    let observer = {
      _c: 0,
      __proto__: NavHistoryObserver.prototype,
      onVisit(uri, id, time, unused, referrerId, transition, guid,
              hidden, visitCount, typed) {
        Assert.ok(testuri.equals(uri));
        Assert.ok(id > 0);
        Assert.ok(time > 0);
        Assert.ok(!hidden);
        do_check_guid_for_uri(uri, guid);
        switch (++this._c) {
          case 1:
            Assert.equal(referrerId, 0);
            Assert.equal(transition, TRANSITION_LINK);
            Assert.equal(visitCount, 1);
            Assert.equal(typed, 0);
            break;
          case 2:
            Assert.ok(referrerId > 0);
            Assert.equal(transition, TRANSITION_LINK);
            Assert.equal(visitCount, 2);
            Assert.equal(typed, 0);
            break;
          case 3:
            Assert.equal(referrerId, 0);
            Assert.equal(transition, TRANSITION_TYPED);
            Assert.equal(visitCount, 3);
            Assert.equal(typed, 1);

            PlacesUtils.history.removeObserver(observer, false);
            resolve();
            break;
        }
      }
    };
    PlacesUtils.history.addObserver(observer, false);
  });
  yield PlacesTestUtils.addVisits([
    { uri: testuri, transition: TRANSITION_LINK },
    { uri: testuri, referrer: testuri, transition: TRANSITION_LINK },
    { uri: testuri, transition: TRANSITION_TYPED },
  ]);
  yield promiseNotifications;
});

add_task(function* test_onDeleteURI() {
  let promiseNotify = onNotify(function onDeleteURI(aURI, aGUID, aReason) {
    Assert.ok(aURI.equals(testuri));
    // Can't use do_check_guid_for_uri() here because the visit is already gone.
    Assert.equal(aGUID, testguid);
    Assert.equal(aReason, Ci.nsINavHistoryObserver.REASON_DELETED);
  });
  let [testuri] = yield task_add_visit();
  let testguid = do_get_guid_for_uri(testuri);
  yield PlacesUtils.history.remove(testuri);
  yield promiseNotify;
});

add_task(function* test_onDeleteVisits() {
  let promiseNotify = onNotify(function onDeleteVisits(aURI, aVisitTime, aGUID,
                                                       aReason) {
    Assert.ok(aURI.equals(testuri));
    // Can't use do_check_guid_for_uri() here because the visit is already gone.
    Assert.equal(aGUID, testguid);
    Assert.equal(aReason, Ci.nsINavHistoryObserver.REASON_DELETED);
    Assert.equal(aVisitTime, 0); // All visits have been removed.
  });
  let msecs24hrsAgo = Date.now() - (86400 * 1000);
  let [testuri] = yield task_add_visit(undefined, msecs24hrsAgo * 1000);
  // Add a bookmark so the page is not removed.
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       testuri,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "test");
  let testguid = do_get_guid_for_uri(testuri);
  yield PlacesUtils.history.remove(testuri);
  yield promiseNotify;
});

add_task(function* test_onTitleChanged() {
  let promiseNotify = onNotify(function onTitleChanged(aURI, aTitle, aGUID) {
    Assert.ok(aURI.equals(testuri));
    Assert.equal(aTitle, title);
    do_check_guid_for_uri(aURI, aGUID);
  });

  let [testuri] = yield task_add_visit();
  let title = "test-title";
  yield PlacesTestUtils.addVisits({
    uri: testuri,
    title
  });
  yield promiseNotify;
});

add_task(function* test_onPageChanged() {
  let promiseNotify = onNotify(function onPageChanged(aURI, aChangedAttribute,
                                                      aNewValue, aGUID) {
    Assert.equal(aChangedAttribute, Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON);
    Assert.ok(aURI.equals(testuri));
    Assert.equal(aNewValue, SMALLPNG_DATA_URI.spec);
    do_check_guid_for_uri(aURI, aGUID);
  });

  let [testuri] = yield task_add_visit();

  // The new favicon for the page must have data associated with it in order to
  // receive the onPageChanged notification.  To keep this test self-contained,
  // we use an URI representing the smallest possible PNG file.
  PlacesUtils.favicons.setAndFetchFaviconForPage(testuri, SMALLPNG_DATA_URI,
                                                 false,
                                                 PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
                                                 null,
                                                 Services.scriptSecurityManager.getSystemPrincipal());
  yield promiseNotify;
});

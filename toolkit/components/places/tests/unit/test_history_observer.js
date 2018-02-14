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
  onTitleChanged() { },
  onDeleteURI() { },
  onClearHistory() { },
  onPageChanged() { },
  onDeleteVisits() { },
  QueryInterface: ChromeUtils.generateQI([Ci.nsINavHistoryObserver])
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
    PlacesUtils.history.addObserver(obs);
  });
}

/**
 * Registers a one-time places observer for 'page-visited',
 * which resolves a promise on being called.
 */
function promiseVisitAdded(callback) {
  return new Promise(resolve => {
    function listener(events) {
      PlacesObservers.removeListener(["page-visited"], listener);
      Assert.equal(events.length, 1, "Right number of visits notified");
      Assert.equal(events[0].type, "page-visited");
      callback(events[0]);
      resolve();
    }
    PlacesObservers.addListener(["page-visited"], listener);
  });
}

/**
 * Asynchronous task that adds a visit to the history database.
 */
async function task_add_visit(uri, timestamp, transition) {
  uri = uri || NetUtil.newURI("http://firefox.com/");
  timestamp = timestamp || Date.now() * 1000;
  await PlacesTestUtils.addVisits({
    uri,
    transition: transition || TRANSITION_TYPED,
    visitDate: timestamp
  });
  return [uri, timestamp];
}

add_task(async function test_visitAdded() {
  let promiseNotify = promiseVisitAdded(function(visit) {
    Assert.ok(visit.visitId > 0);
    Assert.equal(visit.url, testuri.spec);
    Assert.equal(visit.visitTime, testtime / 1000);
    Assert.equal(visit.referringVisitId, 0);
    Assert.equal(visit.transitionType, TRANSITION_TYPED);
    let uri = NetUtil.newURI(visit.url);
    do_check_guid_for_uri(uri, visit.pageGuid);
    Assert.ok(!visit.hidden);
    Assert.equal(visit.visitCount, 1);
    Assert.equal(visit.typedCount, 1);
  });
  let testuri = NetUtil.newURI("http://firefox.com/");
  let testtime = Date.now() * 1000;
  await task_add_visit(testuri, testtime);
  await promiseNotify;
});

add_task(async function test_visitAdded() {
  let promiseNotify = promiseVisitAdded(function(visit) {
    Assert.ok(visit.visitId > 0);
    Assert.equal(visit.url, testuri.spec);
    Assert.equal(visit.visitTime, testtime / 1000);
    Assert.equal(visit.referringVisitId, 0);
    Assert.equal(visit.transitionType, TRANSITION_FRAMED_LINK);
    let uri = NetUtil.newURI(visit.url);
    do_check_guid_for_uri(uri, visit.pageGuid);
    Assert.ok(visit.hidden);
    Assert.equal(visit.visitCount, 1);
    Assert.equal(visit.typedCount, 0);
  });
  let testuri = NetUtil.newURI("http://hidden.firefox.com/");
  let testtime = Date.now() * 1000;
  await task_add_visit(testuri, testtime, TRANSITION_FRAMED_LINK);
  await promiseNotify;
});

add_task(async function test_multiple_onVisit() {
  let testuri = NetUtil.newURI("http://self.firefox.com/");
  let promiseNotifications = new Promise(resolve => {
    function listener(aEvents) {
      Assert.equal(aEvents.length, 3, "Right number of visits notified");
      for (let i = 0; i < aEvents.length; i++) {
        Assert.equal(aEvents[i].type, "page-visited");
        let visit = aEvents[i];
        Assert.equal(testuri.spec, visit.url);
        Assert.ok(visit.visitId > 0);
        Assert.ok(visit.visitTime > 0);
        Assert.ok(!visit.hidden);
        let uri = NetUtil.newURI(visit.url);
        do_check_guid_for_uri(uri, visit.pageGuid);
        switch (i) {
          case 0:
            Assert.equal(visit.referringVisitId, 0);
            Assert.equal(visit.transitionType, TRANSITION_LINK);
            Assert.equal(visit.visitCount, 1);
            Assert.equal(visit.typedCount, 0);
            break;
          case 1:
            Assert.ok(visit.referringVisitId > 0);
            Assert.equal(visit.transitionType, TRANSITION_LINK);
            Assert.equal(visit.visitCount, 2);
            Assert.equal(visit.typedCount, 0);
            break;
          case 2:
            Assert.equal(visit.referringVisitId, 0);
            Assert.equal(visit.transitionType, TRANSITION_TYPED);
            Assert.equal(visit.visitCount, 3);
            Assert.equal(visit.typedCount, 1);

            PlacesObservers.removeListener(["page-visited"], listener);
            resolve();
            break;
        }
      }
    }
    PlacesObservers.addListener(["page-visited"], listener);
  });
  await PlacesTestUtils.addVisits([
    { uri: testuri, transition: TRANSITION_LINK },
    { uri: testuri, referrer: testuri, transition: TRANSITION_LINK },
    { uri: testuri, transition: TRANSITION_TYPED },
  ]);
  await promiseNotifications;
});

add_task(async function test_onDeleteURI() {
  let promiseNotify = onNotify(function onDeleteURI(aURI, aGUID, aReason) {
    Assert.ok(aURI.equals(testuri));
    // Can't use do_check_guid_for_uri() here because the visit is already gone.
    Assert.equal(aGUID, testguid);
    Assert.equal(aReason, Ci.nsINavHistoryObserver.REASON_DELETED);
  });
  let [testuri] = await task_add_visit();
  let testguid = do_get_guid_for_uri(testuri);
  await PlacesUtils.history.remove(testuri);
  await promiseNotify;
});

add_task(async function test_onDeleteVisits() {
  let promiseNotify = onNotify(function onDeleteVisits(aURI, aVisitTime, aGUID,
                                                       aReason) {
    Assert.ok(aURI.equals(testuri));
    // Can't use do_check_guid_for_uri() here because the visit is already gone.
    Assert.equal(aGUID, testguid);
    Assert.equal(aReason, Ci.nsINavHistoryObserver.REASON_DELETED);
    Assert.equal(aVisitTime, 0); // All visits have been removed.
  });
  let msecs24hrsAgo = Date.now() - (86400 * 1000);
  let [testuri] = await task_add_visit(undefined, msecs24hrsAgo * 1000);
  // Add a bookmark so the page is not removed.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test",
    url: testuri,
  });
  let testguid = do_get_guid_for_uri(testuri);
  await PlacesUtils.history.remove(testuri);
  await promiseNotify;
});

add_task(async function test_onTitleChanged() {
  let promiseNotify = onNotify(function onTitleChanged(aURI, aTitle, aGUID) {
    Assert.ok(aURI.equals(testuri));
    Assert.equal(aTitle, title);
    do_check_guid_for_uri(aURI, aGUID);
  });

  let [testuri] = await task_add_visit();
  let title = "test-title";
  await PlacesTestUtils.addVisits({
    uri: testuri,
    title
  });
  await promiseNotify;
});

add_task(async function test_onPageChanged() {
  let promiseNotify = onNotify(function onPageChanged(aURI, aChangedAttribute,
                                                      aNewValue, aGUID) {
    Assert.equal(aChangedAttribute, Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON);
    Assert.ok(aURI.equals(testuri));
    Assert.equal(aNewValue, SMALLPNG_DATA_URI.spec);
    do_check_guid_for_uri(aURI, aGUID);
  });

  let [testuri] = await task_add_visit();

  // The new favicon for the page must have data associated with it in order to
  // receive the onPageChanged notification.  To keep this test self-contained,
  // we use an URI representing the smallest possible PNG file.
  PlacesUtils.favicons.setAndFetchFaviconForPage(testuri, SMALLPNG_DATA_URI,
                                                 false,
                                                 PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
                                                 null,
                                                 Services.scriptSecurityManager.getSystemPrincipal());
  await promiseNotify;
});

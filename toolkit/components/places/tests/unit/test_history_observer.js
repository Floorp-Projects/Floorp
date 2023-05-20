/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Registers a one-time places observer for 'page-visited',
 * which resolves a promise on being called.
 */
function promiseVisitAdded(callback) {
  return new Promise(resolve => {
    async function listener(events) {
      PlacesObservers.removeListener(["page-visited"], listener);
      Assert.equal(events.length, 1, "Right number of visits notified");
      Assert.equal(events[0].type, "page-visited");
      await callback(events[0]);
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
    visitDate: timestamp,
  });
  return [uri, timestamp];
}

add_task(async function test_visitAdded() {
  let promiseNotify = promiseVisitAdded(async function (visit) {
    Assert.ok(visit.visitId > 0);
    Assert.equal(visit.url, testuri.spec);
    Assert.equal(visit.visitTime, testtime / 1000);
    Assert.equal(visit.referringVisitId, 0);
    Assert.equal(visit.transitionType, TRANSITION_TYPED);
    let uri = NetUtil.newURI(visit.url);
    await check_guid_for_uri(uri, visit.pageGuid);
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
  let promiseNotify = promiseVisitAdded(async function (visit) {
    Assert.ok(visit.visitId > 0);
    Assert.equal(visit.url, testuri.spec);
    Assert.equal(visit.visitTime, testtime / 1000);
    Assert.equal(visit.referringVisitId, 0);
    Assert.equal(visit.transitionType, TRANSITION_FRAMED_LINK);
    let uri = NetUtil.newURI(visit.url);
    await check_guid_for_uri(uri, visit.pageGuid);
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
    async function listener(aEvents) {
      Assert.equal(aEvents.length, 3, "Right number of visits notified");
      for (let i = 0; i < aEvents.length; i++) {
        Assert.equal(aEvents[i].type, "page-visited");
        let visit = aEvents[i];
        Assert.equal(testuri.spec, visit.url);
        Assert.ok(visit.visitId > 0);
        Assert.ok(visit.visitTime > 0);
        Assert.ok(!visit.hidden);
        let uri = NetUtil.newURI(visit.url);
        await check_guid_for_uri(uri, visit.pageGuid);
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

add_task(async function test_pageRemovedFromStore() {
  let [testuri] = await task_add_visit();
  let testguid = await PlacesTestUtils.getDatabaseValue("moz_places", "guid", {
    url: testuri,
  });

  const promiseNotify = PlacesTestUtils.waitForNotification("page-removed");

  await PlacesUtils.history.remove(testuri);

  const events = await promiseNotify;
  Assert.equal(events.length, 1, "Right number of page-removed notified");
  Assert.equal(events[0].type, "page-removed");
  Assert.ok(events[0].isRemovedFromStore);
  Assert.equal(events[0].url, testuri.spec);
  Assert.equal(events[0].pageGuid, testguid);
  Assert.equal(events[0].reason, PlacesVisitRemoved.REASON_DELETED);
});

add_task(async function test_pageRemovedAllVisits() {
  const promiseNotify = PlacesTestUtils.waitForNotification("page-removed");

  let msecs24hrsAgo = Date.now() - 86400 * 1000;
  let [testuri] = await task_add_visit(undefined, msecs24hrsAgo * 1000);
  // Add a bookmark so the page is not removed.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test",
    url: testuri,
  });
  let testguid = await PlacesTestUtils.getDatabaseValue("moz_places", "guid", {
    url: testuri,
  });
  await PlacesUtils.history.remove(testuri);

  const events = await promiseNotify;
  Assert.equal(events.length, 1, "Right number of page-removed notified");
  Assert.equal(events[0].type, "page-removed");
  Assert.ok(!events[0].isRemovedFromStore);
  Assert.equal(events[0].url, testuri.spec);
  // Can't use do_check_guid_for_uri() here because the visit is already gone.
  Assert.equal(events[0].pageGuid, testguid);
  Assert.equal(events[0].reason, PlacesVisitRemoved.REASON_DELETED);
  Assert.ok(!events[0].isPartialVisistsRemoval); // All visits have been removed.
});

add_task(async function test_pageTitleChanged() {
  const [testuri] = await task_add_visit();
  const title = "test-title";

  const promiseNotify =
    PlacesTestUtils.waitForNotification("page-title-changed");

  await PlacesTestUtils.addVisits({
    uri: testuri,
    title,
  });

  const events = await promiseNotify;
  Assert.equal(events.length, 1, "Right number of title changed notified");
  Assert.equal(events[0].type, "page-title-changed");
  Assert.equal(events[0].url, testuri.spec);
  Assert.equal(events[0].title, title);
  await check_guid_for_uri(testuri, events[0].pageGuid);
});

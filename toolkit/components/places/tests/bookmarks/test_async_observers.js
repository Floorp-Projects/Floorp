/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test checks that bookmarks service is correctly forwarding async
 * events like visit or favicon additions. */

const NOW = Date.now() * 1000;
let gBookmarkGuids = [];

add_task(async function setup() {
  // Add multiple bookmarks to the same uri.
  gBookmarkGuids.push((await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://book.ma.rk/"
  })).guid);
  gBookmarkGuids.push((await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "http://book.ma.rk/"
  })).guid);
  Assert.equal(gBookmarkGuids.length, 2);
});

add_task(async function test_add_visit() {
  // Add a visit to the bookmark and wait for the observer.
  let guids = new Set(gBookmarkGuids);
  Assert.equal(guids.size, 2);
  let promiseNotifications = PlacesTestUtils.waitForNotification("onItemVisited",
    (id, visitId, time, transition, uri, parentId, guid, parentGuid) => {
      info(`Got a visit notification for ${guid}.`);
      Assert.ok(visitId > 0);
      guids.delete(guid);
      return guids.size == 0;
  });

  await PlacesTestUtils.addVisits({
    uri: "http://book.ma.rk/",
    transition: TRANSITION_TYPED,
    visitDate: NOW
  });
  await promiseNotifications;
});

add_task(async function test_add_icon() {
  // Add a visit to the bookmark and wait for the observer.
  let guids = new Set(gBookmarkGuids);
  Assert.equal(guids.size, 2);
  let promiseNotifications = PlacesTestUtils.waitForNotification("onItemChanged",
    (id, property, isAnno, newValue, lastModified, itemType, parentId, guid) => {
      info(`Got a changed notification for ${guid}.`);
      Assert.equal(property, "favicon");
      Assert.ok(!isAnno);
      Assert.equal(newValue, SMALLPNG_DATA_URI.spec);
      Assert.equal(lastModified, 0);
      Assert.equal(itemType, PlacesUtils.bookmarks.TYPE_BOOKMARK);
      guids.delete(guid);
      return guids.size == 0;
  });

  PlacesUtils.favicons.setAndFetchFaviconForPage(NetUtil.newURI("http://book.ma.rk/"),
                                                 SMALLPNG_DATA_URI, true,
                                                 PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
                                                 null,
                                                 Services.scriptSecurityManager.getSystemPrincipal());
  await promiseNotifications;
});

add_task(async function test_remove_page() {
  // Add a visit to the bookmark and wait for the observer.
  let guids = new Set(gBookmarkGuids);
  Assert.equal(guids.size, 2);
  let promiseNotifications = PlacesTestUtils.waitForNotification("onItemChanged",
    (id, property, isAnno, newValue, lastModified, itemType, parentId, guid) => {
      info(`Got a changed notification for ${guid}.`);
      Assert.equal(property, "cleartime");
      Assert.ok(!isAnno);
      Assert.equal(newValue, "");
      Assert.equal(lastModified, 0);
      Assert.equal(itemType, PlacesUtils.bookmarks.TYPE_BOOKMARK);
      guids.delete(guid);
      return guids.size == 0;
  });

  await PlacesUtils.history.remove("http://book.ma.rk/");
  await promiseNotifications;
});

add_task(async function shutdown() {
  // Check that async observers don't try to create async statements after
  // shutdown.  That would cause assertions, since the async thread is gone
  // already.  Note that in such a case the notifications are not fired, so we
  // cannot test for them.
  // Put an history notification that triggers AsyncGetBookmarksForURI between
  // asyncClose() and the actual connection closing.  Enqueuing a main-thread
  // event as a shutdown blocker should ensure it runs before
  // places-connection-closed.
  // Notice this code is not using helpers cause it depends on a very specific
  // order, a change in the helpers code could make this test useless.

  let shutdownClient = PlacesUtils.history.shutdownClient.jsclient;
  shutdownClient.addBlocker("Places Expiration: shutdown",
    function() {
      Services.tm.mainThread.dispatch(() => {
        // WARNING: this is very bad, never use out of testing code.
        PlacesUtils.bookmarks.QueryInterface(Ci.nsINavHistoryObserver)
                             .onPageChanged(NetUtil.newURI("http://book.ma.rk/"),
                                            Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON,
                                            "test", "test");
      }, Ci.nsIThread.DISPATCH_NORMAL);
    });
});

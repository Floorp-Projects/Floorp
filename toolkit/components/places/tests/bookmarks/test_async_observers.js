/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test checks that bookmarks service is correctly forwarding async
 * events like visit or favicon additions. */

let gBookmarkGuids = [];

add_task(async function setup() {
  // Add multiple bookmarks to the same uri.
  gBookmarkGuids.push(
    (
      await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        url: "http://book.ma.rk/",
      })
    ).guid
  );
  gBookmarkGuids.push(
    (
      await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        url: "http://book.ma.rk/",
      })
    ).guid
  );
  Assert.equal(gBookmarkGuids.length, 2);
});

add_task(async function test_add_icon() {
  // Add a visit to the bookmark and wait for the observer.
  let guids = new Set(gBookmarkGuids);
  Assert.equal(guids.size, 2);
  let promiseNotifications = PlacesTestUtils.waitForNotification(
    "favicon-changed",
    events =>
      events.some(
        event =>
          event.url == "http://book.ma.rk/" &&
          event.faviconUrl.startsWith("data:image/png;base64")
      )
  );

  PlacesUtils.favicons.setAndFetchFaviconForPage(
    NetUtil.newURI("http://book.ma.rk/"),
    SMALLPNG_DATA_URI,
    true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    null,
    Services.scriptSecurityManager.getSystemPrincipal()
  );
  await promiseNotifications;
});

add_task(async function test_remove_page() {
  // Add a visit to the bookmark and wait for the observer.
  let guids = new Set(gBookmarkGuids);
  Assert.equal(guids.size, 2);
  let promiseNotifications = PlacesTestUtils.waitForNotification(
    "page-removed",
    events =>
      events.some(
        event =>
          event.url === "http://book.ma.rk/" &&
          !event.isRemovedFromStore &&
          !event.isPartialVisistsRemoval
      )
  );
  await PlacesUtils.history.remove("http://book.ma.rk/");
  await promiseNotifications;
});

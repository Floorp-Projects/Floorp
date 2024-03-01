/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { BookmarkList } = ChromeUtils.importESModule(
  "resource://gre/modules/BookmarkList.sys.mjs"
);

registerCleanupFunction(
  async () => await PlacesUtils.bookmarks.eraseEverything()
);

add_task(async function test_url_tracking() {
  const firstBookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "https://www.example.com/",
  });
  const secondBookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "https://www.reddit.com/",
  });

  let deferredUpdate;
  const bookmarkList = new BookmarkList(
    [
      "https://www.example.com/",
      "https://www.reddit.com/",
      "https://www.youtube.com/",
    ],
    () => deferredUpdate?.resolve()
  );

  async function waitForUpdateBookmarksTask(updateTask) {
    deferredUpdate = Promise.withResolvers();
    await updateTask();
    return deferredUpdate.promise;
  }

  info("Check bookmark status of tracked URLs.");
  equal(await bookmarkList.isBookmark("https://www.example.com/"), true);
  equal(await bookmarkList.isBookmark("https://www.reddit.com/"), true);
  equal(await bookmarkList.isBookmark("https://www.youtube.com/"), false);

  info("Add a bookmark.");
  await waitForUpdateBookmarksTask(() =>
    PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: "https://www.youtube.com/",
    })
  );
  equal(await bookmarkList.isBookmark("https://www.youtube.com/"), true);

  info("Remove a bookmark.");
  await waitForUpdateBookmarksTask(() =>
    PlacesUtils.bookmarks.remove(firstBookmark.guid)
  );
  equal(await bookmarkList.isBookmark("https://www.example.com/"), false);

  info("Update a bookmark's URL.");
  await waitForUpdateBookmarksTask(() =>
    PlacesUtils.bookmarks.update({
      guid: secondBookmark.guid,
      url: "https://www.wikipedia.org/",
    })
  );
  equal(await bookmarkList.isBookmark("https://www.reddit.com/"), false);

  info("Add a bookmark after removing listeners.");
  bookmarkList.removeListeners();
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "https://www.example.com/",
  });

  info("Reinitialize the list and validate bookmark status.");
  bookmarkList.setTrackedUrls(["https://www.example.com/"]);
  bookmarkList.addListeners();
  equal(await bookmarkList.isBookmark("https://www.example.com/"), true);

  info("Cleanup.");
  bookmarkList.removeListeners();
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_no_unnecessary_observer_notifications() {
  const spy = sinon.spy();
  const bookmarkList = new BookmarkList(
    ["https://www.example.com/"],
    spy,
    0,
    0
  );

  info("Add a bookmark with an untracked URL.");
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "https://www.reddit.com/",
  });
  await new Promise(resolve => ChromeUtils.idleDispatch(resolve));
  ok(spy.notCalled, "Observer was not notified.");
  equal(await bookmarkList.isBookmark("https://www.reddit.com"), undefined);

  info("Add a bookmark after removing listeners.");
  bookmarkList.removeListeners();
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "https://www.example.com/",
  });
  await new Promise(resolve => ChromeUtils.idleDispatch(resolve));
  ok(spy.notCalled, "Observer was not notified.");
});

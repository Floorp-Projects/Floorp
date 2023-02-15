/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test whether do batch removal if multiple bookmarks are removed at once.

add_task(async function test_remove_multiple_bookmarks() {
  info("Test for remove multiple bookmarks at once");

  info("Insert multiple bookmarks");
  const testBookmarks = [
    {
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "http://example.com/1",
    },
    {
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "http://example.com/2",
    },
    {
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "http://example.com/3",
    },
  ];
  const bookmarks = await Promise.all(
    testBookmarks.map(bookmark => PlacesUtils.bookmarks.insert(bookmark))
  );
  Assert.equal(
    bookmarks.length,
    testBookmarks.length,
    "All test data are insterted correctly"
  );

  info("Remove multiple bookmarks");
  const onRemoved = PlacesTestUtils.waitForNotification("bookmark-removed");
  await PlacesUtils.bookmarks.remove(bookmarks);
  const events = await onRemoved;

  info("Check whether all bookmark-removed events called at at once or not");
  assertBookmarkRemovedEvents(events, bookmarks);
});

add_task(async function test_remove_folder_with_bookmarks() {
  info("Test for remove a folder that has multiple bookmarks");

  info("Insert a folder");
  const testFolder = {
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  };
  const folder = await PlacesUtils.bookmarks.insert(testFolder);
  Assert.ok(folder, "A folder is inserted correctly");

  info("Insert multiple bookmarks to inserted folder");
  const testBookmarks = [
    {
      parentGuid: folder.guid,
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example.com/1",
    },
    {
      parentGuid: folder.guid,
      url: "http://example.com/2",
    },
    {
      parentGuid: folder.guid,
      url: "http://example.com/3",
    },
  ];
  const bookmarks = await Promise.all(
    testBookmarks.map(bookmark => PlacesUtils.bookmarks.insert(bookmark))
  );
  Assert.equal(
    bookmarks.length,
    testBookmarks.length,
    "All test data are insterted correctly"
  );

  info("Remove the inserted folder");
  const notifiedEvents = [];
  const onRemoved = PlacesTestUtils.waitForNotification(
    "bookmark-removed",
    events => {
      notifiedEvents.push(events);
      return notifiedEvents.length === 2;
    }
  );
  await PlacesUtils.bookmarks.remove(folder);
  await onRemoved;

  info("Check whether all bookmark-removed events called at at once or not");
  const eventsForBookmarks = notifiedEvents[0];
  assertBookmarkRemovedEvents(eventsForBookmarks, bookmarks);

  info("Check whether a bookmark-removed event called for the folder");
  const eventsForFolder = notifiedEvents[1];
  Assert.equal(
    eventsForFolder.length,
    1,
    "The length of notified events is correct"
  );
  Assert.equal(
    eventsForFolder[0].guid,
    folder.guid,
    "The guid of event is correct"
  );
});

function assertBookmarkRemovedEvents(events, expectedBookmarks) {
  Assert.equal(
    events.length,
    expectedBookmarks.length,
    "The length of notified events is correct"
  );

  for (let i = 0; i < events.length; i++) {
    const event = events[i];
    const expectedBookmark = expectedBookmarks[i];
    Assert.equal(
      event.guid,
      expectedBookmark.guid,
      `The guid of events[${i}] is correct`
    );
    Assert.equal(
      event.url,
      expectedBookmark.url,
      `The url of events[${i}] is correct`
    );
  }
}

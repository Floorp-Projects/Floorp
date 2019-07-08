/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var observer = {
  __proto__: NavBookmarkObserver.prototype,

  handlePlacesEvents(events) {
    Assert.equal(events.length, 1, "Should only be 1 event.");
    this._itemAddedId = events[0].id;
    this._itemAddedParent = events[0].parentId;
    this._itemAddedIndex = events[0].index;
  },
  onItemChanged(id, property, isAnnotationProperty, value) {
    this._itemChangedId = id;
    this._itemChangedProperty = property;
    this._itemChanged_isAnnotationProperty = isAnnotationProperty;
    this._itemChangedValue = value;
  },
};
PlacesUtils.bookmarks.addObserver(observer);
observer.handlePlacesEvents = observer.handlePlacesEvents.bind(observer);
PlacesUtils.observers.addListener(
  ["bookmark-added"],
  observer.handlePlacesEvents
);

registerCleanupFunction(function() {
  PlacesUtils.bookmarks.removeObserver(observer);
  PlacesUtils.observers.removeListener(
    ["bookmark-added"],
    observer.handlePlacesEvents
  );
});

// Returns do_check_eq with .getTime() added onto parameters
function do_check_date_eq(t1, t2) {
  return Assert.equal(t1.getTime(), t2.getTime());
}

add_task(async function test_bookmark_update_notifications() {
  // We set times in the past to workaround a timing bug due to virtual
  // machines and the skew between PR_Now() and Date.now(), see bug 427142 and
  // bug 858377 for details.
  const PAST_DATE = new Date(Date.now() - 86400000);

  // Insert a new bookmark.
  let testFolder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "test Folder",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
  });

  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: testFolder.guid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "http://google.com/",
    title: "a bookmark",
  });

  // Sanity check.
  Assert.ok(observer.itemChangedProperty === undefined);

  // Set dateAdded in the past and verify the changes.
  await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    dateAdded: PAST_DATE,
  });

  Assert.equal(observer._itemChangedProperty, "dateAdded");
  Assert.equal(observer._itemChangedValue, PlacesUtils.toPRTime(PAST_DATE));

  // After just inserting, modified should be the same as dateAdded.
  do_check_date_eq(bookmark.lastModified, bookmark.dateAdded);

  let updatedBookmark = await PlacesUtils.bookmarks.fetch({
    guid: bookmark.guid,
  });

  do_check_date_eq(updatedBookmark.dateAdded, PAST_DATE);

  // Set lastModified in the past and verify the changes.
  updatedBookmark = await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    lastModified: PAST_DATE,
  });

  Assert.equal(observer._itemChangedProperty, "lastModified");
  Assert.equal(observer._itemChangedValue, PlacesUtils.toPRTime(PAST_DATE));
  do_check_date_eq(updatedBookmark.lastModified, PAST_DATE);

  // Set bookmark title
  updatedBookmark = await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    title: "Google",
  });

  // Test notifications.
  Assert.equal(
    observer._itemChangedId,
    await PlacesUtils.promiseItemId(bookmark.guid)
  );
  Assert.equal(observer._itemChangedProperty, "title");
  Assert.equal(observer._itemChangedValue, "Google");

  // Check lastModified has been updated.
  Assert.ok(is_time_ordered(PAST_DATE, updatedBookmark.lastModified.getTime()));

  // Check that node properties are updated.
  let root = PlacesUtils.getFolderContents(testFolder.guid).root;
  Assert.equal(root.childCount, 1);
  let childNode = root.getChild(0);

  // confirm current dates match node properties
  Assert.equal(
    PlacesUtils.toPRTime(updatedBookmark.dateAdded),
    childNode.dateAdded
  );
  Assert.equal(
    PlacesUtils.toPRTime(updatedBookmark.lastModified),
    childNode.lastModified
  );

  // Test live update of lastModified when setting title.
  updatedBookmark = await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    lastModified: PAST_DATE,
    title: "Google",
  });

  // Check lastModified has been updated.
  Assert.ok(is_time_ordered(PAST_DATE, childNode.lastModified));
  // Test that node value matches db value.
  Assert.equal(
    PlacesUtils.toPRTime(updatedBookmark.lastModified),
    childNode.lastModified
  );

  // Test live update of the exposed date apis.
  await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    dateAdded: PAST_DATE,
  });
  Assert.equal(childNode.dateAdded, PlacesUtils.toPRTime(PAST_DATE));
  await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    lastModified: PAST_DATE,
  });
  Assert.equal(childNode.lastModified, PlacesUtils.toPRTime(PAST_DATE));

  root.containerOpen = false;
});

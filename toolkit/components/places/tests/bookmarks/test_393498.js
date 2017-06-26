/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var observer = {
  __proto__: NavBookmarkObserver.prototype,

  onItemAdded(id, folder, index) {
    this._itemAddedId = id;
    this._itemAddedParent = folder;
    this._itemAddedIndex = index;
  },
  onItemChanged(id, property, isAnnotationProperty, value) {
    this._itemChangedId = id;
    this._itemChangedProperty = property;
    this._itemChanged_isAnnotationProperty = isAnnotationProperty;
    this._itemChangedValue = value;
  }
};
PlacesUtils.bookmarks.addObserver(observer);

do_register_cleanup(function() {
  PlacesUtils.bookmarks.removeObserver(observer);
});

// Returns do_check_eq with .getTime() added onto parameters
function do_check_date_eq( t1, t2) {
  return do_check_eq(t1.getTime(), t2.getTime()) ;
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
    parentGuid: PlacesUtils.bookmarks.menuGuid
  });

  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: testFolder.guid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "http://google.com/",
    title: "a bookmark"
  });

  // Sanity check.
  do_check_true(observer.itemChangedProperty === undefined);

  // Set dateAdded in the past and verify the changes.
  await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    dateAdded: PAST_DATE
  });

  do_check_eq(observer._itemChangedProperty, "dateAdded");
  do_check_eq(observer._itemChangedValue, PlacesUtils.toPRTime(PAST_DATE));

  // After just inserting, modified should be the same as dateAdded.
  do_check_date_eq(bookmark.lastModified, bookmark.dateAdded);

  let updatedBookmark = await PlacesUtils.bookmarks.fetch({
    guid: bookmark.guid
  });

  do_check_date_eq(updatedBookmark.dateAdded, PAST_DATE);

  // Set lastModified in the past and verify the changes.
  updatedBookmark = await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    lastModified: PAST_DATE
  });

  do_check_eq(observer._itemChangedProperty, "lastModified");
  do_check_eq(observer._itemChangedValue, PlacesUtils.toPRTime(PAST_DATE));
  do_check_date_eq(updatedBookmark.lastModified, PAST_DATE);

  // Set bookmark title
  updatedBookmark = await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    title: "Google"
  });

  // Test notifications.
  do_check_eq(observer._itemChangedId, await PlacesUtils.promiseItemId(bookmark.guid));
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "Google");

  // Check lastModified has been updated.
  do_check_true(is_time_ordered(PAST_DATE,
    updatedBookmark.lastModified.getTime()));

  // Check that node properties are updated.
  let testFolderId = await PlacesUtils.promiseItemId(testFolder.guid);
  let root = PlacesUtils.getFolderContents(testFolderId).root;
  do_check_eq(root.childCount, 1);
  let childNode = root.getChild(0);

  // confirm current dates match node properties
  do_check_eq(PlacesUtils.toPRTime(updatedBookmark.dateAdded),
   childNode.dateAdded);
  do_check_eq(PlacesUtils.toPRTime(updatedBookmark.lastModified),
   childNode.lastModified);

  // Test live update of lastModified when setting title.
  updatedBookmark = await PlacesUtils.bookmarks.update({
    guid: bookmark.guid,
    lastModified: PAST_DATE,
    title: "Google"
  });

  // Check lastModified has been updated.
  do_check_true(is_time_ordered(PAST_DATE, childNode.lastModified));
  // Test that node value matches db value.
  do_check_eq(PlacesUtils.toPRTime(updatedBookmark.lastModified),
   childNode.lastModified);

  // Test live update of the exposed date apis.
  await PlacesUtils.bookmarks.update({guid: bookmark.guid, dateAdded: PAST_DATE});
  do_check_eq(childNode.dateAdded, PlacesUtils.toPRTime(PAST_DATE));
  await PlacesUtils.bookmarks.update({guid: bookmark.guid, lastModified: PAST_DATE})
  do_check_eq(childNode.lastModified, PlacesUtils.toPRTime(PAST_DATE));

  root.containerOpen = false;
});

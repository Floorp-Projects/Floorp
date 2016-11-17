/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var observer = {
  __proto__: NavBookmarkObserver.prototype,

  onItemAdded: function(id, folder, index) {
    this._itemAddedId = id;
    this._itemAddedParent = folder;
    this._itemAddedIndex = index;
  },
  onItemChanged: function(id, property, isAnnotationProperty, value) {
    this._itemChangedId = id;
    this._itemChangedProperty = property;
    this._itemChanged_isAnnotationProperty = isAnnotationProperty;
    this._itemChangedValue = value;
  }
};
PlacesUtils.bookmarks.addObserver(observer, false);

do_register_cleanup(function() {
  PlacesUtils.bookmarks.removeObserver(observer);
});

function run_test() {
  // We set times in the past to workaround a timing bug due to virtual
  // machines and the skew between PR_Now() and Date.now(), see bug 427142 and
  // bug 858377 for details.
  const PAST_PRTIME = (Date.now() - 86400000) * 1000;

  // Insert a new bookmark.
  let testFolder = PlacesUtils.bookmarks.createFolder(
    PlacesUtils.placesRootId, "test Folder",
    PlacesUtils.bookmarks.DEFAULT_INDEX);
  let bookmarkId = PlacesUtils.bookmarks.insertBookmark(
    testFolder, uri("http://google.com/"),
    PlacesUtils.bookmarks.DEFAULT_INDEX, "");

  // Sanity check.
  do_check_true(observer.itemChangedProperty === undefined);

  // Set dateAdded in the past and verify the bookmarks cache.
  PlacesUtils.bookmarks.setItemDateAdded(bookmarkId, PAST_PRTIME);
  do_check_eq(observer._itemChangedProperty, "dateAdded");
  do_check_eq(observer._itemChangedValue, PAST_PRTIME);
  let dateAdded = PlacesUtils.bookmarks.getItemDateAdded(bookmarkId);
  do_check_eq(dateAdded, PAST_PRTIME);

  // After just inserting, modified should be the same as dateAdded.
  do_check_eq(PlacesUtils.bookmarks.getItemLastModified(bookmarkId), dateAdded);

  // Set lastModified in the past and verify the bookmarks cache.
  PlacesUtils.bookmarks.setItemLastModified(bookmarkId, PAST_PRTIME);
  do_check_eq(observer._itemChangedProperty, "lastModified");
  do_check_eq(observer._itemChangedValue, PAST_PRTIME);
  do_check_eq(PlacesUtils.bookmarks.getItemLastModified(bookmarkId),
              PAST_PRTIME);

  // Set bookmark title
  PlacesUtils.bookmarks.setItemTitle(bookmarkId, "Google");

  // Test notifications.
  do_check_eq(observer._itemChangedId, bookmarkId);
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "Google");

  // Check lastModified has been updated.
  is_time_ordered(PAST_PRTIME,
                  PlacesUtils.bookmarks.getItemLastModified(bookmarkId));

  // Check that node properties are updated.
  let root = PlacesUtils.getFolderContents(testFolder).root;
  do_check_eq(root.childCount, 1);
  let childNode = root.getChild(0);

  // confirm current dates match node properties
  do_check_eq(PlacesUtils.bookmarks.getItemDateAdded(bookmarkId),
              childNode.dateAdded);
  do_check_eq(PlacesUtils.bookmarks.getItemLastModified(bookmarkId),
              childNode.lastModified);

  // Test live update of lastModified when setting title.
  PlacesUtils.bookmarks.setItemLastModified(bookmarkId, PAST_PRTIME);
  PlacesUtils.bookmarks.setItemTitle(bookmarkId, "Google");

  // Check lastModified has been updated.
  is_time_ordered(PAST_PRTIME, childNode.lastModified);
  // Test that node value matches db value.
  do_check_eq(PlacesUtils.bookmarks.getItemLastModified(bookmarkId),
              childNode.lastModified);

  // Test live update of the exposed date apis.
  PlacesUtils.bookmarks.setItemDateAdded(bookmarkId, PAST_PRTIME);
  do_check_eq(childNode.dateAdded, PAST_PRTIME);
  PlacesUtils.bookmarks.setItemLastModified(bookmarkId, PAST_PRTIME);
  do_check_eq(childNode.lastModified, PAST_PRTIME);

  root.containerOpen = false;
}

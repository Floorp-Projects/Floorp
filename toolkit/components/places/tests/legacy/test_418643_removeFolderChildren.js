/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get services.
try {
  var histSvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var annoSvc = Cc["@mozilla.org/browser/annotation-service;1"]
                  .getService(Ci.nsIAnnotationService);
} catch (ex) {
  do_throw("Could not get services\n");
}

var validAnnoName = "validAnno";
var validItemName = "validItem";
var deletedAnnoName = "deletedAnno";
var deletedItemName = "deletedItem";
var bookmarkedURI = "http://www.mozilla.org/";
// set lastModified to the past to prevent VM timing bugs
var pastDate = new Date(Date.now() - 1000);
var deletedBookmarkGuids = [];

// bookmarks observer
var observer = {
  // cached ordered array of notified items
  _onItemRemovedItemGuids: [],
  onItemRemoved(itemId, parentId, index, type, uri, guid, parentGuid) {
    // We should first get notifications for children, then for their parent
    Assert.equal(this._onItemRemovedItemGuids.indexOf(parentGuid), -1,
      "should first get notifications for children, then their parents.");
    // Ensure we are not wrongly removing 1 level up
    Assert.notEqual(parentGuid, PlacesUtils.bookmarks.toolbarGuid,
      "should not be removing from the level above");
    // Removed item must be one of those we have manually deleted
    Assert.notEqual(deletedBookmarkGuids.indexOf(guid), -1,
      "should be one of the deleted items.");
    this._onItemRemovedItemGuids.push(itemId);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver])
};

PlacesUtils.bookmarks.addObserver(observer);

async function add_bookmarks() {
  // This is the folder we will cleanup
  let validFolder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: validItemName,
    type: PlacesUtils.bookmarks.TYPE_FOLDER
  });

  let validFolderId = await PlacesUtils.promiseItemId(validFolder.guid);

  annoSvc.setItemAnnotation(validFolderId, validAnnoName,
                            "annotation", 0,
                            annoSvc.EXPIRE_NEVER);
  await PlacesUtils.bookmarks.update({
    guid: validFolder.guid,
    dateAdded: pastDate,
    lastModified: pastDate,
  });

  // This bookmark should not be deleted
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: validItemName,
    url: bookmarkedURI,
  });

  let validItemId = await PlacesUtils.promiseItemId(bookmark.guid);
  annoSvc.setItemAnnotation(validItemId, validAnnoName,
                            "annotation", 0, annoSvc.EXPIRE_NEVER);

  // The following contents should be deleted
  bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: validFolder.guid,
    title: deletedItemName,
    url: bookmarkedURI,
  });

  let deletedItemId = await PlacesUtils.promiseItemId(bookmark.guid);

  annoSvc.setItemAnnotation(deletedItemId, deletedAnnoName,
                            "annotation", 0, annoSvc.EXPIRE_NEVER);
  deletedBookmarkGuids.push(bookmark.guid);

  let internalFolder = await PlacesUtils.bookmarks.insert({
    parentGuid: validFolder.guid,
    title: deletedItemName,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  let internalFolderId = await PlacesUtils.promiseItemId(internalFolder.guid);

  annoSvc.setItemAnnotation(internalFolderId, deletedAnnoName,
                            "annotation", 0, annoSvc.EXPIRE_NEVER);
  deletedBookmarkGuids.push(internalFolder.guid);

  bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: internalFolder.guid,
    title: deletedItemName,
    url: bookmarkedURI,
  });

  deletedItemId = await PlacesUtils.promiseItemId(bookmark.guid);

  annoSvc.setItemAnnotation(deletedItemId, deletedAnnoName,
                            "annotation", 0, annoSvc.EXPIRE_NEVER);
  deletedBookmarkGuids.push(bookmark.guid);

  return [validFolder.guid, validFolderId];
}

async function check_bookmarks(folderGuid) {
  // check that we still have valid bookmarks
  let bookmarks = [];
  await PlacesUtils.bookmarks.fetch({url: bookmarkedURI}, bookmark => bookmarks.push(bookmark));

  Assert.equal(bookmarks.length, 1, "Should have the correct amount of bookmarks.");

  Assert.equal(bookmarks[0].title, validItemName);
  let id = await PlacesUtils.promiseItemId(bookmarks[0].guid);
  Assert.ok(annoSvc.itemHasAnnotation(id, validAnnoName));

  // check that folder exists and has still its annotation
  let folder = await PlacesUtils.bookmarks.fetch(folderGuid);
  Assert.equal(folder.title, validItemName, "should have the correct name");
  let folderId = await PlacesUtils.promiseItemId(folderGuid);
  Assert.ok(annoSvc.itemHasAnnotation(folderId, validAnnoName),
    "should still have an annotation");

  // test that lastModified got updated
  Assert.ok(pastDate < folder.lastModified, "lastModified date should have been updated");

  // check that folder is empty
  var options = histSvc.getNewQueryOptions();
  var query = histSvc.getNewQuery();
  query.setFolders([folderId], 1);
  var result = histSvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 0);
  root.containerOpen = false;

  // test that all children have been deleted, we use annos for that
  var deletedItems = annoSvc.getItemsWithAnnotation(deletedAnnoName);
  Assert.equal(deletedItems.length, 0);

  // test that observer has been called for (and only for) deleted items
  Assert.equal(observer._onItemRemovedItemGuids.length, deletedBookmarkGuids.length);

  // Sanity check: all roots should be intact.
  folder = await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.rootGuid);
  Assert.equal(folder.parentGuid, undefined, "root shouldn't have a parent.");
  folder = await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.menuGuid);
  Assert.equal(folder.parentGuid, PlacesUtils.bookmarks.rootGuid,
    "bookmarks menu should still be a child of root.");
  folder = await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.tagsGuid);
  Assert.equal(folder.parentGuid, PlacesUtils.bookmarks.rootGuid,
    "tags folder should still be a child of root.");
  folder = await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(folder.parentGuid, PlacesUtils.bookmarks.rootGuid,
    "unfiled folder should still be a child of root.");
  folder = await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.toolbarGuid);
  Assert.equal(folder.parentGuid, PlacesUtils.bookmarks.rootGuid,
    "toolbar folder should still be a child of root.");
}

add_task(async function test_removing_folder_children() {
  let [folderGuid, folderId] = await add_bookmarks();
  PlacesUtils.bookmarks.removeFolderChildren(folderId);
  await check_bookmarks(folderGuid);
});
